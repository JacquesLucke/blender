#include "BLI_timeit.hpp"
#include "BLI_multimap.hpp"

#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"

namespace BParticles {

using BLI::MultiMap;
using BLI::ValueOrError;

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
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (is_emitter_socket(vsocket)) {
      return vsocket;
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

  Set<std::string> particle_type_names;
  StringMap<AttributesDeclaration> declarations;

  for (VirtualNode *particle_type_node : get_type_nodes(vtree)) {
    AttributesDeclaration attributes;
    attributes.add_float3("Position", {0, 0, 0});
    attributes.add_float3("Velocity", {0, 0, 0});
    attributes.add_float("Size", 0.01f);
    attributes.add_float3("Color", {1.0f, 1.0f, 1.0f});
    declarations.add_new(particle_type_node->name(), attributes);
    particle_type_names.add_new(particle_type_node->name());
  }

  ValueOrError<VTreeDataGraph> data_graph_or_error = FN::DataFlowNodes::generate_graph(vtree);
  if (data_graph_or_error.is_error()) {
    return {};
  }

  VTreeDataGraph data_graph = data_graph_or_error.extract_value();

  BuildContext ctx = {data_graph, particle_type_names, world_state};

  MultiMap<std::string, Force *> forces;
  for (auto item : get_force_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->output(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto force = item.value(ctx, vnode);
          if (force) {
            forces.add(linked->vnode()->name(), force.release());
          }
        }
      }
    }
  }

  MultiMap<std::string, OffsetHandler *> offset_handlers;
  for (auto item : get_offset_handler_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->output(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto listener = item.value(ctx, vnode);
          if (listener) {
            offset_handlers.add(linked->vnode()->name(), listener.release());
          }
        }
      }
    }
  }

  MultiMap<std::string, Event *> events;
  for (auto item : get_event_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      for (VirtualSocket *linked : vnode->input(0)->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto event = item.value(ctx, vnode);
          if (event) {
            events.add(linked->vnode()->name(), event.release());
          }
        }
      }
    }
  }

  Vector<Emitter *> emitters;
  for (auto item : get_emitter_builders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      VirtualSocket *emitter_output = find_emitter_output(vnode);
      for (VirtualSocket *linked : emitter_output->links()) {
        if (is_particle_type_node(linked->vnode())) {
          auto emitter = item.value(ctx, vnode, linked->vnode()->name());
          if (emitter) {
            emitters.append(emitter.release());
          }
        }
      }
    }
  }

  StringMap<ParticleType *> particle_types;
  for (VirtualNode *vnode : get_type_nodes(vtree)) {
    std::string name = vnode->name();
    ArrayRef<Force *> forces_on_type = forces.lookup_default(name);

    Integrator *integrator = nullptr;
    if (forces_on_type.size() == 0) {
      integrator = new ConstantVelocityIntegrator();
    }
    else {
      integrator = new EulerIntegrator(forces_on_type);
    }

    ParticleType *particle_type = new ParticleType(declarations.lookup_ref(name),
                                                   integrator,
                                                   events.lookup_default(name),
                                                   offset_handlers.lookup_default(name));
    particle_types.add_new(name, particle_type);
  }

  StepDescription *step_description = new StepDescription(time_step, particle_types, emitters);
  return std::unique_ptr<StepDescription>(step_description);
}

}  // namespace BParticles
