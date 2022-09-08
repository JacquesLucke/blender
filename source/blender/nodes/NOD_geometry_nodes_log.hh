/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <chrono>

#include "BLI_compute_context.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_attribute.h"
#include "BKE_geometry_set.hh"

#include "FN_field.hh"

#include "DNA_node_types.h"

struct SpaceNode;
struct NodesModifierData;

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

class ViewerNodeLog {
 public:
  GeometrySet geometry;
  GField field;
};

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class GeoTreeLogger {
 public:
  std::optional<ComputeContextHash> parent_hash;
  std::optional<std::string> group_node_name;
  Vector<ComputeContextHash> children_hashes;

  LinearAllocator<> *allocator = nullptr;
  Vector<std::pair<std::string, NodeWarning>> node_warnings;
  Vector<destruct_ptr<ValueLog>> socket_values_owner;
  Vector<std::tuple<std::string, std::string, ValueLog *>> input_socket_values;
  Vector<std::tuple<std::string, std::string, ValueLog *>> output_socket_values;
  Vector<std::tuple<std::string, TimePoint, TimePoint>> node_execution_times;
  Vector<std::tuple<std::string, destruct_ptr<ViewerNodeLog>>, 0> viewer_node_logs_;
  Vector<std::tuple<std::string, std::string, NamedAttributeUsage>, 0> used_named_attributes_;

  GeoTreeLogger();
  ~GeoTreeLogger();
  void log_value(const bNode &node, const bNodeSocket &socket, GPointer value);
  void log_viewer_node(const bNode &viewer_node, const GeometrySet &geometry, const GField &field);
};

class GeoNodeLog {
 public:
  Vector<NodeWarning> warnings;
  std::chrono::nanoseconds run_time{0};
  Map<std::string, ValueLog *> input_values_;
  Map<std::string, ValueLog *> output_values_;
  Map<std::string, NamedAttributeUsage> used_named_attributes;

  GeoNodeLog();
  ~GeoNodeLog();
};

class GeoModifierLog;

class GeoTreeLog {
 private:
  GeoModifierLog *modifier_log_;
  Vector<GeoTreeLogger *> tree_loggers_;
  bool reduced_node_warnings_ = false;
  bool reduced_node_run_times_ = false;
  bool reduced_socket_values_ = false;
  bool reduced_viewer_node_logs_ = false;
  bool reduced_existing_attributes_ = false;
  bool reduced_used_named_attributes_ = false;

 public:
  Map<std::string, GeoNodeLog> nodes;
  Map<std::string, ViewerNodeLog *, 0> viewer_node_logs;
  Vector<NodeWarning> all_warnings;
  std::chrono::nanoseconds run_time_sum{0};
  Vector<const GeometryAttributeInfo *> existing_attributes;
  Map<std::string, NamedAttributeUsage> used_named_attributes;

  GeoTreeLog(GeoModifierLog *modifier_log, Vector<GeoTreeLogger *> tree_loggers);
  ~GeoTreeLog();

  void ensure_node_warnings();
  void ensure_node_run_time();
  void ensure_socket_values();
  void ensure_viewer_node_logs();
  void ensure_existing_attributes();
  void ensure_used_named_attributes();

  ValueLog *find_socket_value_log(const bNodeSocket &query_socket);
};

class GeoModifierLog {
 private:
  struct LocalData {
    LinearAllocator<> allocator;
    Map<ComputeContextHash, destruct_ptr<GeoTreeLogger>> tree_logger_by_context;
  };

  threading::EnumerableThreadSpecific<LocalData> data_per_thread_;
  Map<ComputeContextHash, std::unique_ptr<GeoTreeLog>> tree_logs_;

 public:
  GeoTreeLogger &get_local_tree_logger(const ComputeContext &compute_context);
  GeoTreeLog &get_tree_log(const ComputeContextHash &compute_context_hash);

  struct ObjectAndModifier {
    const Object *object;
    const NodesModifierData *nmd;
  };

  static std::optional<ObjectAndModifier> get_modifier_for_node_editor(const SpaceNode &snode);
  static GeoTreeLog *get_tree_log_for_node_editor(const SpaceNode &snode);
};

}  // namespace blender::nodes::geo_eval_log
