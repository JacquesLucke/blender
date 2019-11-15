#pragma once

#include "FN_vtree_multi_function_network.h"
#include "FN_multi_function_common_contexts.h"

#include "particle_function.hpp"

namespace BParticles {

using BKE::VNode;
using FN::VTreeMFNetwork;

Optional<std::unique_ptr<ParticleFunction>> create_particle_function(
    const VNode &vnode, VTreeMFNetwork &data_graph, FN::ExternalDataCacheContext &data_cache);

}  // namespace BParticles
