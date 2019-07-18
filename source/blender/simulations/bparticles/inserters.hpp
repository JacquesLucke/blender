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
using BLI::StringMap;
using FN::DataFlowNodes::BTreeDataGraph;

struct BuildContext {
  IndexedNodeTree &indexed_tree;
  BTreeDataGraph &data_graph;
  ModifierStepDescription &step_description;
  WorldState &world_state;
};

using ForceFromNodeCallback =
    std::function<std::unique_ptr<Force>(BuildContext &ctx, bNode *bnode)>;

StringMap<ForceFromNodeCallback> &get_force_builders();

using EventFromNodeCallback =
    std::function<std::unique_ptr<Event>(BuildContext &ctx, bNode *bnode)>;

StringMap<EventFromNodeCallback> &get_event_builders();

using EmitterFromNodeCallback = std::function<std::unique_ptr<Emitter>(
    BuildContext &ctx, bNode *bnode, StringRef particle_type_name)>;

StringMap<EmitterFromNodeCallback> &get_emitter_builders();

using OffsetHandlerFromNodeCallback =
    std::function<std::unique_ptr<OffsetHandler>(BuildContext &ctx, bNode *bnode)>;

StringMap<OffsetHandlerFromNodeCallback> &get_offset_handler_builders();

}  // namespace BParticles
