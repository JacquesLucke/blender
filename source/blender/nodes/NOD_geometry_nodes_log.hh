/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_context_stack_map.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

// enum class NodeWarningType {
//   Error,
//   Warning,
//   Info,
// };

// struct NodeWithWarning {
//   const bNode *orig_node;
//   NodeWarningType type;
//   std::string message;
// };

// class GeoNodesEvalLog {
//  private:
//   Vector<NodeWithWarning> node_warnings_;

//  public:
//   void log_node_warning(const bNode &orig_node, const NodeWarningType type, std::string message)
//   {
//     node_warnings_.append({&orig_node, type, message});
//   }
// };

}  // namespace blender::nodes
