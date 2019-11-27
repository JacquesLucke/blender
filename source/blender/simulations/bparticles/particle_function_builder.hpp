#pragma once

#include "FN_inlined_tree_multi_function_network.h"
#include "FN_multi_function_common_contexts.h"

#include "particle_function.hpp"

namespace BParticles {

using BKE::XNode;
using FN::VTreeMFNetwork;

Optional<std::unique_ptr<ParticleFunction>> create_particle_function(
    const XNode &xnode, VTreeMFNetwork &data_graph, FN::ExternalDataCacheContext &data_cache);

}  // namespace BParticles
