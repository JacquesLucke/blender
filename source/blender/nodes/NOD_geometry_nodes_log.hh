/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_context_stack_map.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_attribute.h"

#include "DNA_node_types.h"

namespace blender::nodes::geo_eval_log {

enum class NodeWarningType {
  Error,
  Warning,
  Info,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

enum class NamedAttributeUsage {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Remove = 1 << 2,
};
ENUM_OPERATORS(NamedAttributeUsage, NamedAttributeUsage::Remove);

class ValueLog {
 public:
  virtual ~ValueLog() = default;
};

class GenericValueLog : public ValueLog {
 private:
  GMutablePointer data_;

 public:
  GenericValueLog(const GMutablePointer data) : data_(data)
  {
  }

  ~GenericValueLog()
  {
    data_.destruct();
  }

  GPointer value() const
  {
    return data_;
  }
};

struct GeometryAttributeInfo {
  std::string name;
  /** Can be empty when #name does not actually exist on a geometry yet. */
  std::optional<eAttrDomain> domain;
  std::optional<eCustomDataType> data_type;
};

class GeoNodesTreeEvalLog {
 public:
  LinearAllocator<> allocator;
  Vector<std::pair<std::string, NodeWarning>> node_warnings;
  Vector<destruct_ptr<ValueLog>> socket_values_owner;
  Vector<std::tuple<std::string, std::string, ValueLog *>> input_socket_values;
  Vector<std::tuple<std::string, std::string, ValueLog *>> output_socket_values;
};

class ReducedGeoNodeEvalLog {
 public:
  Vector<NodeWarning> warnings;
};

class GeoNodesModifierEvalLog;

class ReducedGeoNodesTreeEvalLog {
 private:
  Vector<GeoNodesTreeEvalLog *> tree_logs_;
  bool reduced_node_warnings_ = false;

  friend GeoNodesModifierEvalLog;

 public:
  Map<std::string, ReducedGeoNodeEvalLog> nodes;

  void ensure_node_warnings()
  {
    if (reduced_node_warnings_) {
      return;
    }
    for (GeoNodesTreeEvalLog *tree_log : tree_logs_) {
      for (const std::pair<std::string, NodeWarning> &warnings : tree_log->node_warnings) {
        this->nodes.lookup_or_add_default(warnings.first).warnings.append(warnings.second);
      }
    }
    reduced_node_warnings_ = true;
  }
};

class GeoNodesModifierEvalLog {
 private:
  threading::EnumerableThreadSpecific<ContextStackMap<GeoNodesTreeEvalLog>> log_map_per_thread_;
  ContextStackMap<ReducedGeoNodesTreeEvalLog> reduced_log_map_;

 public:
  GeoNodesTreeEvalLog &get_local_log(const ContextStack &context_stack)
  {
    return log_map_per_thread_.local().lookup_or_add(context_stack);
  }

  ReducedGeoNodesTreeEvalLog &get_reduced_tree_log(const ContextStack &context_stack)
  {
    ReducedGeoNodesTreeEvalLog &reduced_tree_log = reduced_log_map_.lookup_or_add(context_stack);
    if (reduced_tree_log.tree_logs_.is_empty()) {
      for (ContextStackMap<GeoNodesTreeEvalLog> &log_map : log_map_per_thread_) {
        GeoNodesTreeEvalLog *tree_log = log_map.lookup_ptr(context_stack);
        if (tree_log != nullptr) {
          reduced_tree_log.tree_logs_.append(tree_log);
        }
      }
    }
    return reduced_tree_log;
  }
};

}  // namespace blender::nodes::geo_eval_log
