#pragma once

#include "BKE_node_tree.hpp"

#include "world_state.hpp"
#include "step_simulator.hpp"

namespace BParticles {

using BKE::VirtualNodeTree;

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree);

}  // namespace BParticles
