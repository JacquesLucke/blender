#pragma once

#include <functional>

#include "BKE_node_tree.hpp"
#include "FN_data_flow_nodes.hpp"
#include "BLI_string_map.hpp"

#include "world_state.hpp"
#include "step_description.hpp"
#include "forces.hpp"

namespace BParticles {

using BKE::bSocketList;
using BKE::IndexedNodeTree;
using BKE::SocketWithNode;
using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::StringMap;
using FN::DataFlowNodes::BTreeDataGraph;

struct BuildContext {
  BTreeDataGraph &data_graph;
  Set<std::string> &particle_type_names;
  WorldState &world_state;

  bool type_name_exists(StringRef name)
  {
    return this->particle_type_names.contains(name.to_std_string());
  }
};

using ForceFromNodeCallback =
    std::function<std::unique_ptr<Force>(BuildContext &ctx, VirtualNode *vnode)>;

StringMap<ForceFromNodeCallback> &get_force_builders();

using EventFromNodeCallback =
    std::function<std::unique_ptr<Event>(BuildContext &ctx, VirtualNode *vnode)>;

StringMap<EventFromNodeCallback> &get_event_builders();

using EmitterFromNodeCallback = std::function<std::unique_ptr<Emitter>(
    BuildContext &ctx, VirtualNode *vnode, StringRef particle_type_name)>;

StringMap<EmitterFromNodeCallback> &get_emitter_builders();

using OffsetHandlerFromNodeCallback =
    std::function<std::unique_ptr<OffsetHandler>(BuildContext &ctx, VirtualNode *vnode)>;

StringMap<OffsetHandlerFromNodeCallback> &get_offset_handler_builders();

}  // namespace BParticles
