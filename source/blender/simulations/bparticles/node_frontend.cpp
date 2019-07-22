#include "BLI_timeit.hpp"
#include "BLI_small_multimap.hpp"

#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"

namespace BParticles {

using BLI::SmallMultiMap;

static bool is_particle_type_node(VirtualNode *vnode)
{
  return STREQ(vnode->bnode()->idname, "bp_ParticleTypeNode");
}

static bool is_emitter_socket(VirtualSocket *vsocket)
{
  return STREQ(vsocket->bsocket()->idname, "bp_EmitterSocket");
}

static VirtualSocket *find_emitter_output(VirtualNode *vnode)
{
  for (VirtualSocket &vsocket : vnode->outputs()) {
    if (is_emitter_socket(&vsocket)) {
      return &vsocket;
    }
  }
  BLI_assert(false);
  return nullptr;
}

static ArrayRef<VirtualNode *> get_type_nodes(VirtualNodeTree &vtree)
{
  return vtree.nodes_with_idname("bp_ParticleTypeNode");
}

std::unique_ptr<StepDescription> step_description_from_node_tree(VirtualNodeTree &vtree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  SCOPED_TIMER(__func__);

  StepDescriptionBuilder step_builder;

  for (VirtualNode *particle_type_node : get_type_nodes(vtree)) {
    auto &type_builder = step_builder.add_type(particle_type_node->bnode()->name);
    auto &attributes = type_builder.attributes();
    attributes.add_float3("Position", {0, 0, 0});
    attributes.add_float3("Velocity", {0, 0, 0});
    attributes.add_float("Size", 0.01f);
    attributes.add_float3("Color", {1.0f, 1.0f, 1.0f});
  }

  auto data_graph = FN::DataFlowNodes::generate_graph(vtree).value();

  BuildContext ctx = {data_graph, step_builder, world_state};

  SmallMultiMap<std::string, Force *> forces;
  for (auto item : get_force_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->output(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto force = item.value(ctx, vnode);
          if (force) {
            forces.add(linked->vnode()->bnode()->name, force.release());
          }
        }
      }
    }
  }

  for (auto item : get_offset_handler_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->output(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto listener = item.value(ctx, vnode);
          if (listener) {
            step_builder.get_type(linked->vnode()->bnode()->name)
                .add_offset_handler(std::move(listener));
          }
        }
      }
    }
  }

  for (auto item : get_event_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->input(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto event = item.value(ctx, vnode);
          if (event) {
            step_builder.get_type(linked->vnode()->bnode()->name).add_event(std::move(event));
          }
        }
      }
    }
  }

  for (auto item : get_emitter_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      VirtualSocket *emitter_output = find_emitter_output(vnode);
      for (VirtualSocket *linked : emitter_output->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto emitter = item.value(ctx, vnode, linked->vnode()->bnode()->name);
          if (emitter) {
            step_builder.add_emitter(std::move(emitter));
          }
        }
      }
    }
  }

  for (VirtualNode *vnode : get_type_nodes(vtree)) {
    std::string name = vnode->bnode()->name;
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
