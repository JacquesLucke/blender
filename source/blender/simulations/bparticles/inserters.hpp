#pragma once

#include <functional>

#include "BKE_node_tree.hpp"

#include "world_state.hpp"
#include "step_description.hpp"

namespace BParticles {

using BKE::IndexedNodeTree;

ModifierStepDescription *step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                         WorldState &world_state);

}  // namespace BParticles
