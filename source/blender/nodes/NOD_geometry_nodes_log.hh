/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <chrono>

#include "BLI_context_stack_map.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_attribute.h"
#include "BKE_geometry_set.hh"

#include "FN_field.hh"

#include "DNA_node_types.h"

namespace blender::nodes::geo_eval_log {

using fn::GField;

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
 public:
  GMutablePointer value;

  GenericValueLog(const GMutablePointer value) : value(value)
  {
  }

  ~GenericValueLog()
  {
    this->value.destruct();
  }
};

class FieldInfoLog : public ValueLog {
 public:
  const CPPType &type;
  Vector<std::string> input_tooltips;

  FieldInfoLog(const GField &field);
};

struct GeometryAttributeInfo {
  std::string name;
  /** Can be empty when #name does not actually exist on a geometry yet. */
  std::optional<eAttrDomain> domain;
  std::optional<eCustomDataType> data_type;
};

class GeometryInfoLog : public ValueLog {
 public:
  Vector<GeometryAttributeInfo> attributes;
  Vector<GeometryComponentType> component_types;

  struct MeshInfo {
    int verts_num, edges_num, faces_num;
  };
  struct CurveInfo {
    int splines_num;
  };
  struct PointCloudInfo {
    int points_num;
  };
  struct InstancesInfo {
    int instances_num;
  };
  struct EditDataInfo {
    bool has_deformed_positions;
    bool has_deform_matrices;
  };

  std::optional<MeshInfo> mesh_info;
  std::optional<CurveInfo> curve_info;
  std::optional<PointCloudInfo> pointcloud_info;
  std::optional<InstancesInfo> instances_info;
  std::optional<EditDataInfo> edit_data_info;

  GeometryInfoLog(const GeometrySet &geometry_set);
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

  void log_value(const bNode &node, const bNodeSocket &socket, GPointer value);
};

class GeoNodeLog {
 public:
  Vector<NodeWarning> warnings;
  std::chrono::nanoseconds run_time{0};
  Map<std::string, ValueLog *> input_values_;
  Map<std::string, ValueLog *> output_values_;
};

class GeoModifierLog;

class GeoTreeLog {
 private:
  GeoModifierLog *modifier_log_;
  Vector<GeoTreeLogger *> tree_loggers_;
  bool reduced_node_warnings_ = false;
  bool reduced_node_run_times_ = false;
  bool reduced_socket_values_ = false;

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
  void ensure_socket_values();
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
