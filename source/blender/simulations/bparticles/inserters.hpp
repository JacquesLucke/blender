#pragma once

#include <functional>

#include "BKE_node_tree.hpp"
#include "FN_data_flow_nodes.hpp"
#include "BLI_string_map.hpp"
#include "BLI_value_or_error.hpp"
#include "BLI_multi_map.hpp"

#include "world_state.hpp"
#include "step_description.hpp"
#include "forces.hpp"
#include "particle_function.hpp"

namespace BParticles {

using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::MultiMap;
using BLI::StringMap;
using BLI::ValueOrError;
using FN::DataFlowNodes::VTreeDataGraph;

struct BuildContext {
  VTreeDataGraph &data_graph;
  Set<std::string> &particle_type_names;
  WorldState &world_state;

  bool type_name_exists(StringRef name)
  {
    return this->particle_type_names.contains(name.to_std_string());
  }
};

using ForceFromNodeCallback = std::function<std::unique_ptr<Force>(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)>;

using EventFromNodeCallback = std::function<std::unique_ptr<Event>(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)>;

using EmitterFromNodeCallback = std::function<std::unique_ptr<Emitter>(
    BuildContext &ctx, VirtualNode *vnode, StringRef particle_type_name)>;

using OffsetHandlerFromNodeCallback = std::function<std::unique_ptr<OffsetHandler>(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)>;

StringMap<ForceFromNodeCallback> &get_force_builders();
StringMap<EventFromNodeCallback> &get_event_builders();
StringMap<EmitterFromNodeCallback> &get_emitter_builders();
StringMap<OffsetHandlerFromNodeCallback> &get_offset_handler_builders();

class Components {
 public:
  MultiMap<std::string, Force *> m_forces;
  MultiMap<std::string, OffsetHandler *> m_offset_handlers;
  MultiMap<std::string, Event *> m_events;
  Vector<Emitter *> m_emitters;

  void register_force(StringRef particle_type, std::unique_ptr<Force> force)
  {
    m_forces.add(particle_type.to_std_string(), force.release());
  }

  void register_offset_handler(StringRef particle_type,
                               std::unique_ptr<OffsetHandler> offset_handler)
  {
    m_offset_handlers.add(particle_type.to_std_string(), offset_handler.release());
  }

  void register_event(StringRef particle_type, std::unique_ptr<Event> event)
  {
    m_events.add(particle_type.to_std_string(), event.release());
  }

  void register_emitter(std::unique_ptr<Emitter> emitter)
  {
    m_emitters.append(emitter.release());
  }
};

using ComponentLoader =
    std::function<void(BuildContext &ctx, Components &components, VirtualNode *vnode)>;

StringMap<ComponentLoader> &get_component_loaders();

}  // namespace BParticles
