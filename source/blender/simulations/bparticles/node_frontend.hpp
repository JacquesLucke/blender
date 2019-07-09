#pragma once

#include "BKE_node_tree.hpp"

#include "world_state.hpp"
#include "step_description.hpp"

namespace BParticles {

using BKE::IndexedNodeTree;

std::unique_ptr<StepDescription> step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                                 WorldState &world_state,
                                                                 float time_step);

}  // namespace BParticles
