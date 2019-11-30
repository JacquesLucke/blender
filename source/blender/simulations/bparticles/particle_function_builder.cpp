#include "BLI_lazy_init_cxx.h"
#include "BLI_hash.h"

#include "particle_function_builder.hpp"

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

static Vector<const MFInputSocket *> find_input_data_sockets(const XNode &xnode,
                                                             VTreeMFNetwork &data_graph)
{
  Vector<const MFInputSocket *> inputs;
  for (const XInputSocket *xsocket : xnode.inputs()) {
    if (data_graph.is_mapped(*xsocket)) {
      const MFInputSocket &socket = data_graph.lookup_dummy_socket(*xsocket);
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
    dependencies.append(&data_graph.lookup_xsocket(*socket));
  }

  return dependencies;
}

using BuildInputProvider = std::function<ParticleFunctionInputProvider *(
    VTreeMFNetwork &inlined_tree_data_graph, const XOutputSocket &xsocket)>;

BLI_LAZY_INIT_STATIC(StringMap<BuildInputProvider>, get_input_providers_map)
{
  StringMap<BuildInputProvider> map;
  return map;
}

static ParticleFunctionInputProvider *create_input_provider(
    VTreeMFNetwork &inlined_tree_data_graph, const XOutputSocket &xsocket)
{
  const XNode &xnode = xsocket.node();

  auto &map = get_input_providers_map();
  auto &builder = map.lookup(xnode.idname());
  ParticleFunctionInputProvider *provider = builder(inlined_tree_data_graph, xsocket);
  BLI_assert(provider != nullptr);
  return provider;
}

static Optional<std::unique_ptr<ParticleFunction>> create_particle_function_from_sockets(
    VTreeMFNetwork &data_graph,
    ArrayRef<const MFInputSocket *> sockets_to_compute,
    ArrayRef<const XOutputSocket *> dependencies,
    FN::ExternalDataCacheContext &data_cache,
    FN::PersistentSurfacesLookupContext &persistent_surface_lookup)
{
  Vector<const MFOutputSocket *> dependency_sockets;
  Vector<ParticleFunctionInputProvider *> input_providers;

  for (const XOutputSocket *xsocket : dependencies) {
    dependency_sockets.append(&data_graph.lookup_socket(*xsocket));
    input_providers.append(create_input_provider(data_graph, *xsocket));
  }

  std::unique_ptr<FN::MultiFunction> fn = BLI::make_unique<FN::MF_EvaluateNetwork>(
      dependency_sockets, sockets_to_compute);

  return BLI::make_unique<ParticleFunction>(
      std::move(fn), input_providers, data_cache, persistent_surface_lookup);
}

Optional<std::unique_ptr<ParticleFunction>> create_particle_function(
    const XNode &xnode,
    VTreeMFNetwork &data_graph,
    FN::ExternalDataCacheContext &data_cache,
    FN::PersistentSurfacesLookupContext &persistent_surface_lookup)
{
  Vector<const MFInputSocket *> sockets_to_compute = find_input_data_sockets(xnode, data_graph);
  auto dependencies = find_particle_dependencies(data_graph, sockets_to_compute);

  return create_particle_function_from_sockets(
      data_graph, sockets_to_compute, dependencies, data_cache, persistent_surface_lookup);
}

}  // namespace BParticles
