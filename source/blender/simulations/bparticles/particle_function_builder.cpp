#include "FN_llvm.hpp"

#include "particle_function_builder.hpp"
#include "particle_function_input_providers.hpp"

#include "events.hpp"
#include "action_contexts.hpp"

namespace BParticles {

using BKE::VirtualSocket;
using BLI::float2;
using BLI::rgba_b;
using FN::DataSocket;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::SharedDataGraph;
using FN::SharedFunction;
using FN::Type;

Vector<DataSocket> find_input_data_sockets(VirtualNode *vnode, VTreeDataGraph &data_graph)
{
  Vector<DataSocket> inputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    DataSocket *socket = data_graph.lookup_socket_ptr(vsocket);
    if (socket != nullptr) {
      inputs.append(*socket);
    }
  }
  return inputs;
}

static SetVector<VirtualSocket *> find_particle_dependencies(
    VTreeDataGraph &data_graph,
    ArrayRef<DataSocket> sockets,
    MutableArrayRef<bool> r_depends_on_particle_flags)
{
  SetVector<VirtualSocket *> combined_dependencies;

  for (uint i = 0; i < sockets.size(); i++) {
    DataSocket socket = sockets[i];
    auto dependencies = data_graph.find_placeholder_dependencies({socket});
    bool has_dependency = dependencies.size() > 0;
    r_depends_on_particle_flags[i] = has_dependency;

    combined_dependencies.add_multiple(dependencies);
  }

  return combined_dependencies;
}

static AttributeType attribute_type_from_socket_type(FN::Type *type)
{
  if (type == FN::Types::TYPE_float3) {
    return AttributeType::Float3;
  }
  else if (type == FN::Types::TYPE_float) {
    return AttributeType::Float;
  }
  else if (type == FN::Types::TYPE_int32) {
    return AttributeType::Integer;
  }
  else {
    BLI_assert(false);
    return AttributeType::Byte;
  }
}

static ParticleFunctionInputProvider *create_input_provider(VirtualSocket *vsocket,
                                                            SharedDataGraph &data_graph,
                                                            DataSocket socket)
{
  VirtualNode *vnode = vsocket->vnode();
  if (STREQ(vnode->idname(), "bp_ParticleInfoNode")) {
    if (STREQ(vsocket->name(), "Age")) {
      return new AgeInputProvider();
    }
    else {
      return new AttributeInputProvider(
          attribute_type_from_socket_type(data_graph->type_of_socket(socket)), vsocket->name());
    }
  }
  else if (STREQ(vnode->idname(), "bp_CollisionInfoNode")) {
    return new CollisionNormalInputProvider();
  }
  else if (STREQ(vnode->idname(), "bp_SurfaceImageNode")) {
    PointerRNA rna = vnode->rna();
    Image *image = (Image *)RNA_pointer_get(&rna, "image").data;
    BLI_assert(image != nullptr);
    return new SurfaceImageInputProvider(image);
  }
  else {
    BLI_assert(false);
    return nullptr;
  }
}

static SharedFunction create_function__with_deps(
    VTreeDataGraph &data_graph,
    StringRef function_name,
    ArrayRef<DataSocket> sockets_to_compute,
    ArrayRef<VirtualSocket *> input_vsockets,
    MutableArrayRef<ParticleFunctionInputProvider *> r_input_providers)
{
  uint input_amount = input_vsockets.size();
  BLI_assert(input_amount == r_input_providers.size());

  Vector<DataSocket> input_sockets = data_graph.lookup_sockets(input_vsockets);

  FunctionBuilder fn_builder;
  fn_builder.add_inputs(data_graph.graph(), input_sockets);
  fn_builder.add_outputs(data_graph.graph(), sockets_to_compute);

  for (uint i = 0; i < input_amount; i++) {
    r_input_providers[i] = create_input_provider(
        input_vsockets[i], data_graph.graph(), input_sockets[i]);
  }

  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(data_graph.graph(), input_sockets, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  FN::fgraph_add_LLVMBuildIRBody(fn, fgraph);
  return fn;
}

static SharedFunction create_function__without_deps(SharedDataGraph &graph,
                                                    StringRef function_name,
                                                    ArrayRef<DataSocket> sockets_to_compute)
{
  FunctionBuilder fn_builder;
  fn_builder.add_outputs(graph, sockets_to_compute);
  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(graph, {}, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<std::unique_ptr<ParticleFunction>> create_particle_function_from_sockets(
    VTreeDataGraph &data_graph,
    StringRef name,
    ArrayRef<DataSocket> sockets_to_compute,
    ArrayRef<bool> depends_on_particle_flags,
    ArrayRef<VirtualSocket *> dependencies)
{
  Vector<DataSocket> sockets_with_deps;
  Vector<DataSocket> sockets_without_deps;
  for (uint i = 0; i < sockets_to_compute.size(); i++) {
    if (depends_on_particle_flags[i]) {
      sockets_with_deps.append(sockets_to_compute[i]);
    }
    else {
      sockets_without_deps.append(sockets_to_compute[i]);
    }
  }

  Vector<ParticleFunctionInputProvider *> input_providers(dependencies.size(), nullptr);

  SharedFunction fn_without_deps = create_function__without_deps(
      data_graph.graph(), name, sockets_without_deps);
  SharedFunction fn_with_deps = create_function__with_deps(
      data_graph, name, sockets_with_deps, dependencies, input_providers);

  ParticleFunction *particle_fn = new ParticleFunction(
      fn_without_deps, fn_with_deps, input_providers, depends_on_particle_flags);
  return std::unique_ptr<ParticleFunction>(particle_fn);
}

ValueOrError<std::unique_ptr<ParticleFunction>> create_particle_function(
    VirtualNode *vnode, VTreeDataGraph &data_graph)
{
  Vector<DataSocket> sockets_to_compute = find_input_data_sockets(vnode, data_graph);
  Vector<bool> depends_on_particle_flags(sockets_to_compute.size());
  auto dependencies = find_particle_dependencies(
      data_graph, sockets_to_compute, depends_on_particle_flags);

  std::string name = vnode->name() + StringRef(" Inputs");
  return create_particle_function_from_sockets(
      data_graph, name, sockets_to_compute, depends_on_particle_flags, dependencies);
}

}  // namespace BParticles
