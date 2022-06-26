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
  std::optional<ContextStackHash> parent_hash;
  std::optional<std::string> group_node_name;
  Vector<ContextStackHash> children_hashes;

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
  GeoNodesModifierEvalLog *full_log_;
  Vector<GeoNodesTreeEvalLog *> tree_logs_;
  bool reduced_node_warnings_ = false;

 public:
  Map<std::string, ReducedGeoNodeEvalLog> nodes;
  Vector<NodeWarning> all_warnings;

  ReducedGeoNodesTreeEvalLog(GeoNodesModifierEvalLog *full_log,
                             Vector<GeoNodesTreeEvalLog *> tree_logs)
      : full_log_(full_log), tree_logs_(std::move(tree_logs))
  {
  }

  void ensure_node_warnings();
};

class GeoNodesModifierEvalLog {
 private:
  threading::EnumerableThreadSpecific<Map<ContextStackHash, std::unique_ptr<GeoNodesTreeEvalLog>>>
      log_map_per_thread_;
  Map<ContextStackHash, std::unique_ptr<ReducedGeoNodesTreeEvalLog>> reduced_log_map_;

 public:
  GeoNodesTreeEvalLog &get_local_log(const ContextStack &context_stack);
  ReducedGeoNodesTreeEvalLog &get_reduced_tree_log(const ContextStackHash &context_stack);
};

}  // namespace blender::nodes::geo_eval_log
