#include "particle_function_builder.hpp"

namespace BParticles {

using BKE::VirtualSocket;
using FN::DFGraphSocket;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::SharedDataFlowGraph;
using FN::SharedFunction;
using FN::SharedType;

Vector<DFGraphSocket> find_input_data_sockets(VirtualNode *vnode, VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> inputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    DFGraphSocket *socket = data_graph.lookup_socket_ptr(vsocket);
    if (socket != nullptr) {
      inputs.append(*socket);
    }
  }
  return inputs;
}

struct SocketDependencies {
  SetVector<DFGraphSocket> sockets;
  SetVector<VirtualSocket *> vsockets;
};

static SocketDependencies find_particle_dependencies(VTreeDataGraph &data_graph,
                                                     ArrayRef<DFGraphSocket> sockets,
                                                     ArrayRef<bool> r_depends_on_particle)
{
  SocketDependencies combined_dependencies;

  for (uint i = 0; i < sockets.size(); i++) {
    DFGraphSocket socket = sockets[i];
    auto dependencies = data_graph.find_placeholder_dependencies(socket);
    bool has_dependency = dependencies.size() > 0;
    r_depends_on_particle[i] = has_dependency;

    combined_dependencies.sockets.add_multiple(dependencies.sockets);
    combined_dependencies.vsockets.add_multiple(dependencies.vsockets);
    BLI_assert(combined_dependencies.sockets.size() == combined_dependencies.vsockets.size());
  }

  return combined_dependencies;
}

static SharedFunction create_function__with_deps(SharedDataFlowGraph &graph,
                                                 StringRef function_name,
                                                 ArrayRef<DFGraphSocket> sockets_to_compute,
                                                 SocketDependencies &dependencies)
{
  FunctionBuilder fn_builder;
  fn_builder.add_outputs(graph, sockets_to_compute);

  for (uint i = 0; i < dependencies.sockets.size(); i++) {
    VirtualSocket *vsocket = dependencies.vsockets[i];
    DFGraphSocket socket = dependencies.sockets[i];
    VirtualNode *vnode = vsocket->vnode();

    SharedType &type = graph->type_of_output(socket);
    StringRef name_prefix;
    if (STREQ(vnode->idname(), "bp_ParticleInfoNode")) {
      name_prefix = "Attribute: ";
    }
    else if (STREQ(vnode->idname(), "bp_CollisionInfoNode")) {
      name_prefix = "Action Context: ";
    }
    else {
      BLI_assert(false);
    }
    BLI_STRINGREF_STACK_COMBINE(name, name_prefix, vsocket->name());
    fn_builder.add_input(name, type);
  }

  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(graph, dependencies.sockets, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);

  return fn;
}

static SharedFunction create_function__without_deps(SharedDataFlowGraph &graph,
                                                    StringRef function_name,
                                                    ArrayRef<DFGraphSocket> sockets_to_compute)
{
  FunctionBuilder fn_builder;
  fn_builder.add_outputs(graph, sockets_to_compute);
  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(graph, {}, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

ValueOrError<ParticleFunction> create_particle_function(VirtualNode *main_vnode,
                                                        VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(main_vnode, data_graph);
  Vector<bool> parameter_depends_on_particle(sockets_to_compute.size());
  auto dependencies = find_particle_dependencies(
      data_graph, sockets_to_compute, parameter_depends_on_particle);

  Vector<DFGraphSocket> sockets_with_deps;
  Vector<DFGraphSocket> sockets_without_deps;
  for (uint i = 0; i < sockets_to_compute.size(); i++) {
    if (parameter_depends_on_particle[i]) {
      sockets_with_deps.append(sockets_to_compute[i]);
    }
    else {
      sockets_without_deps.append(sockets_to_compute[i]);
    }
  }

  SharedFunction fn_without_deps = create_function__without_deps(
      data_graph.graph(), main_vnode->name(), sockets_without_deps);
  SharedFunction fn_with_deps = create_function__with_deps(
      data_graph.graph(), main_vnode->name(), sockets_with_deps, dependencies);

  ParticleFunction particle_function(fn_without_deps, fn_with_deps, parameter_depends_on_particle);
  return particle_function;
}

}  // namespace BParticles
