#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"

namespace BParticles {

std::unique_ptr<StepDescription> step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  auto node_processors = get_node_processors();

  ModifierStepDescription *step_description = new ModifierStepDescription();

  for (bNode *particle_type_node : indexed_tree.nodes_with_idname("bp_ParticleTypeNode")) {
    ModifierParticleType *type = new ModifierParticleType();
    type->m_integrator = new EulerIntegrator();

    std::string type_name = particle_type_node->name;
    step_description->m_types.add_new(type_name, type);
    step_description->m_particle_type_names.append(type_name);
  }

  auto data_graph = FN::DataFlowNodes::generate_graph(indexed_tree).value();

  for (auto item : node_processors.items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      ProcessNodeInterface interface(
          bnode, indexed_tree, data_graph, world_state, *step_description);
      item.value(interface);
    }
  }

  step_description->m_duration = time_step;
  return std::unique_ptr<StepDescription>(step_description);
}

}  // namespace BParticles
