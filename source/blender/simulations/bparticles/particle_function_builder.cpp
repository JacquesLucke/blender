#include "BLI_lazy_init_cxx.h"
#include "BLI_hash.h"

#include "particle_function_builder.hpp"
#include "particle_function_input_providers.hpp"

#include "events.hpp"
#include "action_contexts.hpp"

#include "FN_multi_functions.h"

namespace BParticles {

using BKE::XInputSocket;
using BKE::XOutputSocket;
using BLI::float2;
using BLI::rgba_b;
using FN::MFInputSocket;
using FN::MFOutputSocket;

static Vector<const MFInputSocket *> find_input_data_sockets(const XNode &vnode,
                                                             VTreeMFNetwork &data_graph)
{
  Vector<const MFInputSocket *> inputs;
  for (const XInputSocket *vsocket : vnode.inputs()) {
    if (data_graph.is_mapped(*vsocket)) {
      const MFInputSocket &socket = data_graph.lookup_dummy_socket(*vsocket);
      inputs.append(&socket);
    }
  }
  return inputs;
}

static VectorSet<const XOutputSocket *> find_particle_dependencies(
    VTreeMFNetwork &data_graph, ArrayRef<const MFInputSocket *> sockets)
{
  Vector<const MFOutputSocket *> dummy_dependencies = data_graph.network().find_dummy_dependencies(
      sockets);

  Vector<const XOutputSocket *> dependencies;
  for (const MFOutputSocket *socket : dummy_dependencies) {
    dependencies.append(&data_graph.lookup_vsocket(*socket));
  }

  return dependencies;
}

using BuildInputProvider = std::function<ParticleFunctionInputProvider *(
    VTreeMFNetwork &vtree_data_graph, const XOutputSocket &vsocket)>;

static ParticleFunctionInputProvider *INPUT_surface_info(VTreeMFNetwork &UNUSED(vtree_data_graph),
                                                         const XOutputSocket &vsocket)
{
  if (vsocket.name() == "Normal") {
    return new SurfaceNormalInputProvider();
  }
  else if (vsocket.name() == "Velocity") {
    return new SurfaceVelocityInputProvider();
  }
  else {
    BLI_assert(false);
    return nullptr;
  }
}

static ParticleFunctionInputProvider *INPUT_surface_image(VTreeMFNetwork &UNUSED(vtree_data_graph),
                                                          const XOutputSocket &vsocket)
{
  Optional<std::string> uv_map_name;

  PointerRNA *rna = vsocket.node().rna();
  Image *image = (Image *)RNA_pointer_get(rna, "image").data;
  BLI_assert(image != nullptr);

  return new SurfaceImageInputProvider(image, uv_map_name);
}

static ParticleFunctionInputProvider *INPUT_surface_weight(
    VTreeMFNetwork &UNUSED(vtree_data_graph), const XOutputSocket &vsocket)
{
  PointerRNA *rna = vsocket.node().rna();
  char group_name[65];
  RNA_string_get(rna, "group_name", group_name);
  return new VertexWeightInputProvider(group_name);
}

static ParticleFunctionInputProvider *INPUT_randomness_input(
    VTreeMFNetwork &UNUSED(vtree_data_graph), const XOutputSocket &vsocket)
{
  uint seed = BLI_hash_string(vsocket.node().name().data());
  return new RandomFloatInputProvider(seed);
}

static ParticleFunctionInputProvider *INPUT_is_in_group(VTreeMFNetwork &vtree_data_graph,
                                                        const XOutputSocket &vsocket)
{
  FN::MF_EvaluateNetwork fn({}, {&vtree_data_graph.lookup_dummy_socket(vsocket.node().input(0))});
  FN::MFParamsBuilder params_builder(fn, 1);
  FN::MFContextBuilder context_builder;

  std::string group_name;
  BLI::destruct(&group_name);
  params_builder.add_single_output(ArrayRef<std::string>(&group_name, 1));
  fn.call({0}, params_builder, context_builder);

  return new IsInGroupInputProvider(group_name);
}

BLI_LAZY_INIT_STATIC(StringMap<BuildInputProvider>, get_input_providers_map)
{
  StringMap<BuildInputProvider> map;
  map.add_new("fn_SurfaceInfoNode", INPUT_surface_info);
  map.add_new("fn_SurfaceImageNode", INPUT_surface_image);
  map.add_new("fn_SurfaceWeightNode", INPUT_surface_weight);
  map.add_new("fn_ParticleRandomnessInputNode", INPUT_randomness_input);
  map.add_new("fn_IsInGroupNode", INPUT_is_in_group);
  return map;
}

static ParticleFunctionInputProvider *create_input_provider(VTreeMFNetwork &vtree_data_graph,
                                                            const XOutputSocket &vsocket)
{
  const XNode &vnode = vsocket.node();

  auto &map = get_input_providers_map();
  auto &builder = map.lookup(vnode.idname());
  ParticleFunctionInputProvider *provider = builder(vtree_data_graph, vsocket);
  BLI_assert(provider != nullptr);
  return provider;
}

static Optional<std::unique_ptr<ParticleFunction>> create_particle_function_from_sockets(
    VTreeMFNetwork &data_graph,
    ArrayRef<const MFInputSocket *> sockets_to_compute,
    ArrayRef<const XOutputSocket *> dependencies,
    FN::ExternalDataCacheContext &data_cache)
{
  Vector<const MFOutputSocket *> dependency_sockets;
  Vector<ParticleFunctionInputProvider *> input_providers;

  for (const XOutputSocket *vsocket : dependencies) {
    dependency_sockets.append(&data_graph.lookup_socket(*vsocket));
    input_providers.append(create_input_provider(data_graph, *vsocket));
  }

  std::unique_ptr<FN::MultiFunction> fn = BLI::make_unique<FN::MF_EvaluateNetwork>(
      dependency_sockets, sockets_to_compute);

  return BLI::make_unique<ParticleFunction>(std::move(fn), input_providers, data_cache);
}

Optional<std::unique_ptr<ParticleFunction>> create_particle_function(
    const XNode &vnode, VTreeMFNetwork &data_graph, FN::ExternalDataCacheContext &data_cache)
{
  Vector<const MFInputSocket *> sockets_to_compute = find_input_data_sockets(vnode, data_graph);
  auto dependencies = find_particle_dependencies(data_graph, sockets_to_compute);

  return create_particle_function_from_sockets(
      data_graph, sockets_to_compute, dependencies, data_cache);
}

}  // namespace BParticles
