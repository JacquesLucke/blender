/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <chrono>

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

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class GeoTreeLogger {
 public:
  std::optional<ContextStackHash> parent_hash;
  std::optional<std::string> group_node_name;
  Vector<ContextStackHash> children_hashes;

  LinearAllocator<> allocator;
  Vector<std::pair<std::string, NodeWarning>> node_warnings;
  Vector<destruct_ptr<ValueLog>> socket_values_owner;
  Vector<std::tuple<std::string, std::string, ValueLog *>> input_socket_values;
  Vector<std::tuple<std::string, std::string, ValueLog *>> output_socket_values;
  Vector<std::tuple<std::string, TimePoint, TimePoint>> node_execution_times;
};

class GeoNodeLog {
 public:
  Vector<NodeWarning> warnings;
  std::chrono::nanoseconds run_time{0};
};

class GeoModifierLog;

class GeoTreeLog {
 private:
  GeoModifierLog *modifier_log_;
  Vector<GeoTreeLogger *> tree_loggers_;
  bool reduced_node_warnings_ = false;
  bool reduced_node_run_times_ = false;

 public:
  Map<std::string, GeoNodeLog> nodes;
  Vector<NodeWarning> all_warnings;
  std::chrono::nanoseconds run_time_sum{0};

  GeoTreeLog(GeoModifierLog *modifier_log, Vector<GeoTreeLogger *> tree_loggers)
      : modifier_log_(modifier_log), tree_loggers_(std::move(tree_loggers))
  {
  }

  void ensure_node_warnings();
  void ensure_node_run_time();
};

class GeoModifierLog {
 private:
  threading::EnumerableThreadSpecific<Map<ContextStackHash, std::unique_ptr<GeoTreeLogger>>>
      tree_loggers_per_thread_;
  Map<ContextStackHash, std::unique_ptr<GeoTreeLog>> tree_logs_;

 public:
  GeoTreeLogger &get_local_tree_logger(const ContextStack &context_stack);
  GeoTreeLog &get_tree_log(const ContextStackHash &context_stack);
};

}  // namespace blender::nodes::geo_eval_log
