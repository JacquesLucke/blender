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

class ReducedGeoNodesTreeEvalLog {
 public:
  bool is_initialized = false;
  Map<std::string, ReducedGeoNodeEvalLog> nodes;
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

  Vector<ContextStackMap<GeoNodesTreeEvalLog> *> log_maps()
  {
    Vector<ContextStackMap<GeoNodesTreeEvalLog> *> logs;
    for (ContextStackMap<GeoNodesTreeEvalLog> &log_map : log_map_per_thread_) {
      logs.append(&log_map);
    }
    return logs;
  }

  ContextStackMap<ReducedGeoNodesTreeEvalLog> &reduced_log_map()
  {
    return reduced_log_map_;
  }
};

}  // namespace blender::nodes::geo_eval_log
