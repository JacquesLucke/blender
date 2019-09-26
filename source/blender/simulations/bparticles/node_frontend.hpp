#pragma once

#include "BKE_virtual_node_tree_cxx.h"

#include "world_state.hpp"
#include "step_simulator.hpp"

namespace BParticles {

using BKE::VirtualNodeTree;

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree);

}  // namespace BParticles
