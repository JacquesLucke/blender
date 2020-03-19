#pragma once

#include "FN_node_tree.h"

#include "step_simulator.hpp"
#include "world_state.hpp"

namespace BParticles {

using FN::FunctionTree;

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree);

}  // namespace BParticles
