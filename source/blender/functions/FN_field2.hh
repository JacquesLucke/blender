/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

namespace blender::fn {
class MultiFunction;
}

namespace blender::fn::lazy_function {
class LazyFunction;
}

namespace blender::fn::field2 {

class FieldFunction;
class FieldNode;
class GField;
class GFieldRef;
template<typename T> class Field;

namespace data_flow_graph {
class Graph;
class Node;
class OutputNode;
class FunctionNode;
class InputSocket;
class OutputSocket;
}  // namespace data_flow_graph

class DfgFunctionBuilder;

class FieldArrayEvaluator;
class FieldArrayEvaluation;
class FieldArrayContext;

enum class BackendFlags {
  None = 0,
  LazyFunction = (1 << 0),
  MultiFunction = (1 << 1),
  ConstantValue = (1 << 2),
};
ENUM_OPERATORS(BackendFlags, BackendFlags::MultiFunction);

class FieldFunction {
 private:
  int inputs_num_;
  int outputs_num_;

 public:
  FieldFunction(int inputs_num, int outputs_num)
      : inputs_num_(inputs_num), outputs_num_(outputs_num)
  {
    BLI_assert(inputs_num_ >= 0);
    BLI_assert(outputs_num_ >= 1);
  }

  int inputs_num() const
  {
    return inputs_num_;
  }

  int outputs_num() const
  {
    return outputs_num_;
  }

  const CPPType &input_cpp_type(const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < inputs_num_);
    return this->input_cpp_type_impl(index);
  }

  const CPPType &output_cpp_type(const int index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < outputs_num_);
    return this->output_cpp_type_impl(index);
  }

  virtual void dfg_build(DfgFunctionBuilder &builder) const = 0;

  virtual std::string dfg_node_name(const void * /*fn_data*/) const
  {
    return "unnamed";
  }

  virtual std::string dfg_input_name(const void * /*fn_data*/, const int /*index*/) const
  {
    return "unnamed";
  }

  virtual std::string dfg_output_name(const void * /*fn_data*/, const int /*index*/) const
  {
    return "unnamed";
  }

  virtual BackendFlags dfg_node_backends(const void * /*fn_data*/) const
  {
    return BackendFlags::None;
  }

  virtual const lazy_function::LazyFunction &dfg_backend_lazy_function(
      const void * /*fn_data*/, ResourceScope & /*scope*/) const
  {
    throw std::runtime_error("lazy-function backend is not supported for this node");
  }

  virtual const MultiFunction &dfg_backend_multi_function(const void * /*fn_data*/,
                                                          ResourceScope & /*scope*/) const
  {
    throw std::runtime_error("multi-function backend is not supported for this node");
  }

  virtual GPointer dfg_backend_constant_value(const void * /*fn_data*/,
                                              ResourceScope & /*scope*/) const
  {
    throw std::runtime_error("constant-value backend is not supported for this node");
  }

 private:
  virtual const CPPType &input_cpp_type_impl(int /*index*/) const
  {
    BLI_assert_unreachable();
    return CPPType::get<int>();
  }
  virtual const CPPType &output_cpp_type_impl(int index) const = 0;
};

template<typename NodePtr> class GFieldBase {
 protected:
  NodePtr node_ = nullptr;
  int index_ = 0;

  GFieldBase(NodePtr node, const int index) : node_(std::move(node)), index_(index)
  {
  }

 public:
  GFieldBase() = default;

  operator bool() const
  {
    return node_ != nullptr;
  }

  friend bool operator==(const GFieldBase &a, const GFieldBase &b)
  {
    return a.node_ == b.node_ && a.index_ == b.index_;
  }

  uint64_t hash() const
  {
    return get_default_hash_2(node_, index_);
  }

  int index() const
  {
    return index_;
  }

  const FieldNode &node() const
  {
    BLI_assert(*this);
    return *node_;
  }

  const FieldNode *node_ptr() const
  {
    return &*node_;
  }

  const CPPType &cpp_type() const
  {
    BLI_assert(*this);
    return node_->output_cpp_type(index_);
  }
};

class GField : public GFieldBase<std::shared_ptr<const FieldNode>> {
 public:
  GField() = default;

  GField(std::shared_ptr<const FieldNode> node, const int index = 0)
      : GFieldBase<std::shared_ptr<const FieldNode>>(std::move(node), index)
  {
  }

  template<typename T> Field<T> typed() const;
};

class GFieldRef : public GFieldBase<const FieldNode *> {
 public:
  GFieldRef() = default;

  GFieldRef(const GField &field) : GFieldBase<const FieldNode *>(field.node_ptr(), field.index())
  {
  }

  GFieldRef(const FieldNode &node, const int index = 0)
      : GFieldBase<const FieldNode *>(&node, index)
  {
  }
};

template<typename T> class Field {
 private:
  GField field_;

 public:
  using base_type = T;

  Field() = default;

  Field(std::shared_ptr<const FieldNode> node, const int index = 0)
      : field_(std::move(node), index)
  {
    BLI_assert(!field_ || field_.cpp_type().is<T>());
  }

  operator const GField &() const
  {
    return field_;
  }

  operator GFieldRef() const
  {
    return field_;
  }

  operator bool() const
  {
    return static_cast<bool>(field_);
  }

  friend bool operator==(const Field &a, const Field &b)
  {
    return a.field_ == b.field_;
  }

  uint64_t hash() const
  {
    return field_.hash();
  }

  int index() const
  {
    return field_.index();
  }

  const FieldNode &node() const
  {
    return field_.node();
  }

  const FieldNode *node_ptr() const
  {
    return field_.node_ptr();
  }
};

/* We want typed and generic fields to have exactly the same memory layout. */
static_assert(sizeof(Field<int>) == sizeof(GField));

template<typename T> inline Field<T> GField::typed() const
{
  return Field<T>(this->node_, this->index_);
}

class FieldNode {
 private:
  std::unique_ptr<const FieldFunction> fn_;
  Vector<GField> inputs_;

 public:
  FieldNode(std::unique_ptr<const FieldFunction> fn, Vector<GField> inputs = {})
      : fn_(std::move(fn)), inputs_(std::move(inputs))
  {
  }

  Span<GField> inputs() const
  {
    return inputs_;
  }

  const FieldFunction &function() const
  {
    return *fn_;
  }

  const CPPType &input_cpp_type(const int index) const
  {
    return fn_->input_cpp_type(index);
  }

  const CPPType &output_cpp_type(const int index) const
  {
    return fn_->output_cpp_type(index);
  }
};

namespace data_flow_graph {

enum class NodeType {
  Output,
  Context,
  Function,
};

class Node {
 protected:
  NodeType type_;
  int inputs_num_;
  int outputs_num_;

  friend Graph;

 public:
  NodeType type() const
  {
    return type_;
  }

  int inputs_num() const
  {
    return inputs_num_;
  }

  int outputs_num() const
  {
    return outputs_num_;
  }
};

class InputSocket {
 public:
  const Node *node = nullptr;
  int index;

  uint64_t hash() const
  {
    return get_default_hash_2(this->node, this->index);
  }

  friend bool operator==(const InputSocket &a, const InputSocket &b)
  {
    return a.node == b.node && a.index == b.index;
  }
};

class OutputSocket {
 public:
  const Node *node = nullptr;
  int index;

  uint64_t hash() const
  {
    return get_default_hash_2(this->node, this->index);
  }

  friend bool operator==(const OutputSocket &a, const OutputSocket &b)
  {
    return a.node == b.node && a.index == b.index;
  }
};

class OutputNode : public Node {
 private:
  const CPPType *cpp_type_ = nullptr;

  friend Graph;

 public:
  const CPPType &cpp_type() const
  {
    return *cpp_type_;
  }
};

class ContextNode : public Node {
 public:
  ContextNode()
  {
    type_ = NodeType::Context;
    inputs_num_ = 0;
    outputs_num_ = 1;
  }
};

class FunctionNode : public Node {
 private:
  const FieldFunction *fn_;
  const void *fn_data_ = nullptr;
  OutputSocket context_;

  friend Graph;

 public:
  const FieldFunction &function() const
  {
    return *fn_;
  }

  const void *fn_data() const
  {
    return fn_data_;
  }

  OutputSocket context() const
  {
    return context_;
  }

  std::string input_name(const int index) const
  {
    return fn_->dfg_input_name(fn_data_, index);
  }

  std::string output_name(const int index) const
  {
    return fn_->dfg_output_name(fn_data_, index);
  }

  std::string name() const
  {
    return fn_->dfg_node_name(fn_data_);
  }
};

class Graph {
 private:
  LinearAllocator<> allocator_;
  ContextNode context_node_;
  Vector<FunctionNode *> function_nodes_;
  Vector<OutputNode *> output_nodes_;
  Map<InputSocket, OutputSocket> origins_map_;
  MultiValueMap<OutputSocket, InputSocket> targets_map_;

 public:
  ~Graph();

  FunctionNode &add_function_node(const OutputSocket &context,
                                  const FieldFunction &fn,
                                  int inputs_num,
                                  int outputs_num,
                                  const void *fn_data);
  OutputNode &add_output_node(const CPPType &cpp_type);

  void add_link(const OutputSocket &from, const InputSocket &to);

  const ContextNode &context_node() const
  {
    return context_node_;
  }

  OutputSocket context_socket() const
  {
    return {&const_cast<ContextNode &>(context_node_), 0};
  }

  Span<const FunctionNode *> function_nodes() const
  {
    return function_nodes_;
  }

  Span<const OutputNode *> output_nodes() const
  {
    return output_nodes_;
  }

  OutputSocket origin_socket(const InputSocket &socket) const
  {
    return origins_map_.lookup(socket);
  }

  std::optional<OutputSocket> origin_socket_opt(const InputSocket &socket) const
  {
    const OutputSocket *output = origins_map_.lookup_ptr(socket);
    if (output == nullptr) {
      return std::nullopt;
    }
    return *output;
  }

  Span<InputSocket> target_sockets(const OutputSocket &socket) const
  {
    return targets_map_.lookup(socket);
  }

  struct ToDotSettings {
    Map<const Node *, uint32_t> cluster_ids_map;
  };

  std::string to_dot(const ToDotSettings &settings = {}) const;
};

}  // namespace data_flow_graph

namespace dfg = data_flow_graph;

class DfgFunctionBuilder {
 public:
  struct InputInfo {
    dfg::InputSocket socket;
    dfg::OutputSocket context;
  };

  struct OutputInfo {
    dfg::OutputSocket socket;
  };

 private:
  dfg::Graph *graph_;
  dfg::OutputSocket context_;
  Vector<InputInfo> r_inputs_;
  Vector<OutputInfo> r_outputs_;

 public:
  DfgFunctionBuilder(dfg::Graph &graph,
                     const dfg::OutputSocket &context,
                     const FieldFunction &function)
      : graph_(&graph),
        context_(context),
        r_inputs_(function.inputs_num()),
        r_outputs_(function.outputs_num())
  {
  }

  dfg::Graph &graph()
  {
    return *graph_;
  }

  dfg::OutputSocket context() const
  {
    return context_;
  }

  void set_input(const int index, const dfg::InputSocket &input_socket)
  {
    r_inputs_[index] = InputInfo{input_socket, context_};
  }

  void set_input(const int index,
                 const dfg::InputSocket &input_socket,
                 const dfg::OutputSocket &input_context)
  {
    r_inputs_[index] = InputInfo{input_socket, input_context};
  }

  void set_output(const int index, const dfg::OutputSocket &output_socket)
  {
    r_outputs_[index] = OutputInfo{output_socket};
  }

  Span<InputInfo> built_inputs() const
  {
    return r_inputs_;
  }

  Span<OutputInfo> built_outputs() const
  {
    return r_outputs_;
  }
};

template<typename T> class ConstantFieldFunction : public FieldFunction {
 private:
  T value_;

 public:
  ConstantFieldFunction(T value) : FieldFunction(0, 1), value_(std::move(value))
  {
  }

  const CPPType &output_cpp_type_impl(const int /*index*/) const override
  {
    return CPPType::get<T>();
  }

  std::string dfg_node_name(const void * /*fn_data*/) const override
  {
    return "constant";
  }

  void dfg_build(DfgFunctionBuilder &builder) const override
  {
    dfg::Graph &graph = builder.graph();
    dfg::FunctionNode &node = graph.add_function_node(builder.context(), *this, 0, 1, nullptr);
    builder.set_output(0, {&node, 0});
  }

  BackendFlags dfg_node_backends(const void * /*fn_data*/) const override
  {
    return BackendFlags::ConstantValue;
  }

  GPointer dfg_backend_constant_value(const void * /*fn_data*/,
                                      ResourceScope & /*scope*/) const override
  {
    return &value_;
  }
};

template<typename T> inline Field<T> make_constant_field(T value)
{
  auto fn = std::make_unique<ConstantFieldFunction<T>>(std::move(value));
  auto node = std::make_shared<FieldNode>(std::move(fn), Vector<GField>{});
  return {std::move(node), 0};
}

Vector<dfg::OutputNode *> build_dfg_for_fields(dfg::Graph &graph, Span<GFieldRef> fields);

class FieldArrayEvaluator {
 private:
  Vector<GFieldRef> fields_;
  dfg::Graph graph_;
  ResourceScope scope_;
  Vector<dfg::OutputNode *> output_nodes_;
  Set<const dfg::Node *> context_dependent_nodes_;
  Vector<int> varying_output_indices_;
  Vector<int> constant_output_indices_;
  Vector<GMutablePointer> constant_outputs_;
  bool is_finalized_ = false;

  friend FieldArrayEvaluation;

 public:
  ~FieldArrayEvaluator();

  int add_field_ref(const GFieldRef field)
  {
    BLI_assert(!is_finalized_);
    return fields_.append_and_get_index(field);
  }

  void finalize();

 private:
  void find_context_dependent_nodes();
  void evaluate_constant_outputs();
  void evaluate_constant_input_socket(const dfg::InputSocket &socket_to_compute,
                                      GMutablePointer r_value);
};

class FieldArrayContext {
 public:
  virtual ~FieldArrayContext() = default;
};

struct FieldArrayContextValue {
  FieldArrayContext *context = nullptr;
  std::shared_ptr<FieldArrayContext> owned_context;
};

class FieldArrayEvaluation {
 private:
  const FieldArrayEvaluator &evaluator_;
  const FieldArrayContext &context_;
  const IndexMask mask_;
  Vector<GVArray> results_;

 public:
  FieldArrayEvaluation(const FieldArrayEvaluator &evaluator,
                       const FieldArrayContext &context,
                       const IndexMask *mask);

  void add_destination(int index, GVMutableArray varray);

  void evaluate();

  const GVArray &get_evaluated(const int index)
  {
    return results_[index];
  }
};

}  // namespace blender::fn::field2
