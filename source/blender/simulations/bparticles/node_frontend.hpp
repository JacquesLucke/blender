#pragma once

#include "BKE_inlined_node_tree.h"

#include "world_state.hpp"
#include "step_simulator.hpp"

namespace BParticles {

using BKE::InlinedNodeTree;

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree);

}  // namespace BParticles
