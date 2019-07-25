#pragma once

#include <functional>

#include "BKE_node_tree.hpp"
#include "FN_data_flow_nodes.hpp"
#include "BLI_string_map.hpp"
#include "BLI_value_or_error.hpp"

#include "world_state.hpp"
#include "step_description.hpp"
#include "forces.hpp"
#include "particle_function.hpp"

namespace BParticles {

using BKE::bSocketList;
using BKE::IndexedNodeTree;
using BKE::SocketWithNode;
using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
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

ValueOrError<SharedFunction> create_function__offset_handler_inputs(
    VirtualNode *offset_handler_vnode, VTreeDataGraph &data_graph);

ValueOrError<SharedFunction> create_function__force_inputs(VirtualNode *force_vnode,
                                                           VTreeDataGraph &data_graph);

ValueOrError<SharedFunction> create_function__event_inputs(VirtualNode *event_vnode,
                                                           VTreeDataGraph &data_graph);

using ForceFromNodeCallback = std::function<std::unique_ptr<Force>(
    BuildContext &ctx, VirtualNode *vnode, ParticleFunction compute_inputs_fn)>;

using EventFromNodeCallback = std::function<std::unique_ptr<Event>(
    BuildContext &ctx, VirtualNode *vnode, ParticleFunction compute_inputs_fn)>;

using EmitterFromNodeCallback = std::function<std::unique_ptr<Emitter>(
    BuildContext &ctx, VirtualNode *vnode, StringRef particle_type_name)>;

using OffsetHandlerFromNodeCallback = std::function<std::unique_ptr<OffsetHandler>(
    BuildContext &ctx, VirtualNode *vnode, ParticleFunction compute_inputs_fn)>;

StringMap<ForceFromNodeCallback> &get_force_builders();
StringMap<EventFromNodeCallback> &get_event_builders();
StringMap<EmitterFromNodeCallback> &get_emitter_builders();
StringMap<OffsetHandlerFromNodeCallback> &get_offset_handler_builders();

}  // namespace BParticles
