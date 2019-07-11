#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"

namespace BParticles {

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

std::unique_ptr<StepDescription> step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  ModifierStepDescription *step_description = new ModifierStepDescription();

  for (bNode *particle_type_node : indexed_tree.nodes_with_idname("bp_ParticleTypeNode")) {
    ModifierParticleType *type = new ModifierParticleType();
    type->m_integrator = new EulerIntegrator();

    std::string type_name = particle_type_node->name;
    step_description->m_types.add_new(type_name, type);
    step_description->m_particle_type_names.append(type_name);
  }

  auto data_graph = FN::DataFlowNodes::generate_graph(indexed_tree).value();

  auto node_processors = get_node_processors();
  for (auto item : node_processors.items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      ProcessNodeInterface interface(
          bnode, indexed_tree, data_graph, world_state, *step_description);
      item.value(interface);
    }
  }

  BuildContext ctx = {indexed_tree, data_graph, *step_description};

  for (auto item : get_force_builders().items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      bNodeSocket *force_output = bSocketList(bnode->outputs).get(0);
      for (SocketWithNode linked : indexed_tree.linked(force_output)) {
        if (is_particle_type_node(linked.node)) {
          auto force = item.value(ctx, bnode);
          if (force) {
            EulerIntegrator *integrator = reinterpret_cast<EulerIntegrator *>(
                step_description->m_types.lookup_ref(linked.node->name)->m_integrator);
            integrator->add_force(std::move(force));
          }
        }
      }
    }
  }

  for (auto item : get_event_builders().items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      bNodeSocket *event_input = bSocketList(bnode->inputs).get(0);
      for (SocketWithNode linked : indexed_tree.linked(event_input)) {
        if (is_particle_type_node(linked.node)) {
          auto event = item.value(ctx, bnode);
          if (event) {
            step_description->m_types.lookup_ref(linked.node->name)
                ->m_events.append(event.release());
          }
        }
      }
    }
  }

  step_description->m_duration = time_step;
  return std::unique_ptr<StepDescription>(step_description);
}

}  // namespace BParticles
