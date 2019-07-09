#include "node_frontend.hpp"
#include "inserters.hpp"

namespace BParticles {

std::unique_ptr<StepDescription> step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  ModifierStepDescription *description = step_description_from_node_tree_impl(indexed_tree,
                                                                              world_state);
  description->m_duration = time_step;
  return std::unique_ptr<StepDescription>(description);
}

}  // namespace BParticles
