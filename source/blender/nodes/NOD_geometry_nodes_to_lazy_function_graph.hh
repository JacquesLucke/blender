/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_lazy_function_graph.hh"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_multi_function.hh"

#include "BLI_context_stack.hh"

struct Object;
struct Depsgraph;

namespace blender::nodes {

namespace lf = fn::lazy_function;
using lf::LazyFunction;
using lf::LazyFunctionGraph;

struct GeoNodesModifierData {
  const Object *self_object = nullptr;
  Depsgraph *depsgraph = nullptr;
  geo_eval_log::GeoModifierLog *eval_log = nullptr;
};

class NodeGroupContextStack : public ContextStack {
 private:
  static constexpr const char *s_static_type = "NODE_GROUP";

  std::string node_name_;
  std::string group_name_;

 public:
  NodeGroupContextStack(const ContextStack *parent, std::string node_name, std::string group_name)
      : ContextStack(s_static_type, parent),
        node_name_(std::move(node_name)),
        group_name_(std::move(group_name))
  {
    hash_.mix_in(s_static_type, strlen(s_static_type));
    hash_.mix_in(node_name_.data(), node_name_.size());
  }

  StringRefNull node_name() const
  {
    return node_name_;
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Node Group: " << group_name_ << " \t Node Name: " << node_name_;
  }
};

class ModifierContextStack : public ContextStack {
 private:
  static constexpr const char *s_static_type = "MODIFIER";

  std::string modifier_name_;

 public:
  ModifierContextStack(const ContextStack *parent, std::string modifier_name)
      : ContextStack(s_static_type, parent), modifier_name_(std::move(modifier_name))
  {
    hash_.mix_in(s_static_type, strlen(s_static_type));
    hash_.mix_in(modifier_name_.data(), modifier_name_.size());
  }

 private:
  void print_current_in_line(std::ostream &stream) const override
  {
    stream << "Modifier: " << modifier_name_;
  }
};

struct GeoNodesLFUserData : public lf::UserData {
  GeoNodesModifierData *modifier_data = nullptr;
  const ContextStack *context_stack = nullptr;
};

struct GeometryNodeLazyFunctionMapping {
  Map<const bNodeSocket *, lf::Socket *> dummy_socket_map;
  Vector<lf::OutputSocket *> group_input_sockets;
};

struct GeometryNodesLazyFunctionResources {
  LinearAllocator<> allocator;
  Vector<std::unique_ptr<LazyFunction>> functions;
  Vector<GMutablePointer> values_to_destruct;
  Vector<std::unique_ptr<NodeMultiFunctions>> node_multi_functions;

  ~GeometryNodesLazyFunctionResources()
  {
    for (GMutablePointer &p : this->values_to_destruct) {
      p.destruct();
    }
  }
};

void geometry_nodes_to_lazy_function_graph(const bNodeTree &tree,
                                           LazyFunctionGraph &graph,
                                           GeometryNodesLazyFunctionResources &resources,
                                           GeometryNodeLazyFunctionMapping &mapping);

}  // namespace blender::nodes
