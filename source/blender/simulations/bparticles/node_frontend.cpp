#include "BLI_timeit.hpp"
#include "BLI_small_multimap.hpp"

#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"

namespace BParticles {

using BLI::SmallMultiMap;

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

static bool is_emitter_socket(bNodeSocket *bsocket)
{
  return STREQ(bsocket->idname, "bp_EmitterSocket");
}

static bNodeSocket *find_emitter_output(bNode *bnode)
{
  for (bNodeSocket *bsocket : bSocketList(bnode->outputs)) {
    if (is_emitter_socket(bsocket)) {
      return bsocket;
    }
  }
  BLI_assert(false);
  return nullptr;
}

static ArrayRef<bNode *> get_type_nodes(IndexedNodeTree &indexed_tree)
{
  return indexed_tree.nodes_with_idname("bp_ParticleTypeNode");
}

std::unique_ptr<StepDescription> step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  SCOPED_TIMER(__func__);

  StepDescriptionBuilder step_builder;

  for (bNode *particle_type_node : get_type_nodes(indexed_tree)) {
    auto &type_builder = step_builder.add_type(particle_type_node->name);
    auto &attributes = type_builder.attributes();
    attributes.add_float3("Position", {0, 0, 0});
    attributes.add_float3("Velocity", {0, 0, 0});
    attributes.add_float("Size", 0.01f);
    attributes.add_float3("Color", {1.0f, 1.0f, 1.0f});
  }

  auto data_graph = FN::DataFlowNodes::generate_graph(indexed_tree).value();

  BuildContext ctx = {indexed_tree, data_graph, step_builder, world_state};

  SmallMultiMap<std::string, Force *> forces;
  for (auto item : get_force_builders().items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      bNodeSocket *force_output = bSocketList(bnode->outputs).get(0);
      for (SocketWithNode linked : indexed_tree.linked(force_output)) {
        if (is_particle_type_node(linked.node)) {
          auto force = item.value(ctx, bnode);
          if (force) {
            forces.add(linked.node->name, force.release());
          }
        }
      }
    }
  }

  for (auto item : get_offset_handler_builders().items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      bNodeSocket *listener_output = bSocketList(bnode->outputs).get(0);
      for (SocketWithNode linked : indexed_tree.linked(listener_output)) {
        if (is_particle_type_node(linked.node)) {
          auto listener = item.value(ctx, bnode);
          if (listener) {
            step_builder.get_type(linked.node->name).add_offset_handler(std::move(listener));
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
            step_builder.get_type(linked.node->name).add_event(std::move(event));
          }
        }
      }
    }
  }

  for (auto item : get_emitter_builders().items()) {
    for (bNode *bnode : indexed_tree.nodes_with_idname(item.key)) {
      bNodeSocket *emitter_output = find_emitter_output(bnode);
      for (SocketWithNode linked : indexed_tree.linked(emitter_output)) {
        if (is_particle_type_node(linked.node)) {
          auto emitter = item.value(ctx, bnode, linked.node->name);
          if (emitter) {
            step_builder.add_emitter(std::move(emitter));
          }
        }
      }
    }
  }

  for (bNode *bnode : get_type_nodes(indexed_tree)) {
    std::string name = bnode->name;
    ParticleTypeBuilder &type_builder = step_builder.get_type(name);
    ArrayRef<Force *> forces_on_type = forces.lookup_default(name);
    if (forces_on_type.size() == 0) {
      type_builder.set_integrator(new ConstantVelocityIntegrator());
    }
    else {
      type_builder.set_integrator(new EulerIntegrator(forces_on_type));
    }
  }

  return step_builder.build(time_step);
}

}  // namespace BParticles
