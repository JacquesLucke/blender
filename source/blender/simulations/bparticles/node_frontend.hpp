#pragma once

#include "BKE_node_tree.hpp"

#include "world_state.hpp"
#include "step_description.hpp"

namespace BParticles {

using BKE::VirtualNodeTree;

std::unique_ptr<StepDescription> step_description_from_node_tree(VirtualNodeTree &vtree,
                                                                 WorldState &world_state,
                                                                 float time_step);

}  // namespace BParticles
