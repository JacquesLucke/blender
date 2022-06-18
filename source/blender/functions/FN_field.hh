/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A #Field represents a function that outputs a value based on an arbitrary number of inputs. The
 * inputs for a specific field evaluation are provided by a context.
 *
 * A typical example is a field that computes a displacement vector for every vertex on a mesh
 * based on its position.
 *
 * Fields can be build, composed and evaluated at run-time. They are stored in a directed tree
 * graph data structure, whereby each node is a #FieldNode and edges are dependencies. A #FieldNode
 * has an arbitrary number of inputs and at least one output and a #Field references a specific
 * output of a #FieldNode. The inputs of a #FieldNode are other fields.
 *
 * There are two different types of field nodes:
 *  - #FieldInput: Has no input and exactly one output. It represents an input to the entire field
 *    when it is evaluated. During evaluation, the value of this input is based on a context.
 *  - #FieldOperation: Has an arbitrary number of field inputs and at least one output. Its main
 *    use is to compose multiple existing fields into new fields.
 *
 * When fields are evaluated, they are converted into a multi-function procedure which allows
 * efficient computation. In the future, we might support different field evaluation mechanisms for
 * e.g. the following scenarios:
 *  - Latency of a single evaluation is more important than throughput.
 *  - Evaluation should happen on other hardware like GPUs.
 *
 * Whenever possible, multiple fields should be evaluated together to avoid duplicate work when
 * they share common sub-fields and a common context.
 */

#include "BLI_function_ref.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "FN_multi_function.hh"

namespace blender::fn {

class FieldInput;
struct FieldInputs;

/**
 * Have a fixed set of base node types, because all code that works with field nodes has to
 * understand those.
 */
enum class FieldNodeType {
  Input,
  Operation,
  Constant,
};

/**
 * A node in a field-tree. It has at least one output that can be referenced by fields.
 */
class FieldNode {
 private:
  FieldNodeType node_type_;
  Vector<const CPPType *> output_types_;

 protected:
  /**
   * Keeps track of the inputs that this node depends on. This avoids recomputing it every time the
   * data is required. It is a shared pointer, because very often multiple nodes depend on the same
   * inputs.
   * Might contain null.
   */
  std::shared_ptr<const FieldInputs> field_inputs_;

 public:
  FieldNode(FieldNodeType node_type, Vector<const CPPType *> output_types);
  virtual ~FieldNode();

  const CPPType &output_cpp_type(const int output_index) const;

  FieldNodeType node_type() const;
  bool depends_on_input() const;

  const std::shared_ptr<const FieldInputs> &field_inputs() const;

  virtual uint64_t hash() const;
  virtual bool is_equal_to(const FieldNode &other) const;
};

/**
 * Common base class for fields to avoid declaring the same methods for #GField and #GFieldRef.
 */
template<typename NodePtr> class GFieldBase {
 protected:
  NodePtr node_ = nullptr;
  int node_output_index_ = 0;

  GFieldBase(NodePtr node, const int node_output_index)
      : node_(std::move(node)), node_output_index_(node_output_index)
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
    /* Two nodes can compare equal even when their pointer is not the same. For example, two
     * "Position" nodes are the same. */
    return *a.node_ == *b.node_ && a.node_output_index_ == b.node_output_index_;
  }

  uint64_t hash() const
  {
    return get_default_hash_2(*node_, node_output_index_);
  }

  const CPPType &cpp_type() const
  {
    return node_->output_cpp_type(node_output_index_);
  }

  const FieldNode &node() const
  {
    return *node_;
  }

  int node_output_index() const
  {
    return node_output_index_;
  }
};

/**
 * A field whose output type is only known at run-time.
 */
class GField : public GFieldBase<std::shared_ptr<FieldNode>> {
 public:
  GField() = default;

  GField(std::shared_ptr<FieldNode> node, const int node_output_index = 0)
      : GFieldBase<std::shared_ptr<FieldNode>>(std::move(node), node_output_index)
  {
  }
};

/**
 * Same as #GField but is cheaper to copy/move around, because it does not contain a
 * #std::shared_ptr.
 */
class GFieldRef : public GFieldBase<const FieldNode *> {
 public:
  GFieldRef() = default;

  GFieldRef(const GField &field)
      : GFieldBase<const FieldNode *>(&field.node(), field.node_output_index())
  {
  }

  GFieldRef(const FieldNode &node, const int node_output_index = 0)
      : GFieldBase<const FieldNode *>(&node, node_output_index)
  {
  }
};

namespace detail {
/* Utility class to make #is_field_v work. */
struct TypedFieldBase {
};
}  // namespace detail

/**
 * A typed version of #GField. It has the same memory layout as #GField.
 */
template<typename T> class Field : public GField, detail::TypedFieldBase {
 public:
  using base_type = T;

  Field() = default;

  Field(GField field) : GField(std::move(field))
  {
    BLI_assert(this->cpp_type().template is<T>());
  }

  Field(std::shared_ptr<FieldNode> node, const int node_output_index = 0)
      : Field(GField(std::move(node), node_output_index))
  {
  }
};

/** True when T is any Field<...> type. */
template<typename T>
static constexpr bool is_field_v = std::is_base_of_v<detail::TypedFieldBase, T> &&
                                   !std::is_same_v<detail::TypedFieldBase, T>;

/**
 * A #FieldNode that allows composing existing fields into new fields.
 */
class FieldOperation : public FieldNode {
 private:
  /** Inputs to the operation. */
  Vector<GField> inputs_;

 public:
  FieldOperation(Vector<GField> inputs, Vector<const CPPType *> output_types);
  ~FieldOperation();

  Span<GField> inputs() const;
};

/**
 * A #FieldNode that represents an input to the entire field-tree.
 */
class FieldInput : public FieldNode {
 public:
  /* The order is also used for sorting in socket inspection. */
  enum class Category {
    NamedAttribute = 0,
    Generated = 1,
    AnonymousAttribute = 2,
    Unknown,
  };

 protected:
  std::string debug_name_;
  Category category_ = Category::Unknown;

 public:
  FieldInput(const CPPType &type, std::string debug_name = "");
  ~FieldInput();

  virtual std::string socket_inspection_name() const;
  blender::StringRef debug_name() const;
  const CPPType &cpp_type() const;
  Category category() const;
};

class FieldConstant : public FieldNode {
 private:
  void *value_;

 public:
  FieldConstant(const CPPType &type, const void *value);
  ~FieldConstant();

  const CPPType &type() const;
  GPointer value() const;
};

/**
 * Keeps track of the inputs of a field.
 */
struct FieldInputs {
  /** All #FieldInput nodes that a field (possibly indirectly) depends on. */
  VectorSet<const FieldInput *> nodes;
  /**
   * Same as above but the inputs are deduplicated. For example, when there are two separate index
   * input nodes, only one will show up in this list.
   */
  VectorSet<std::reference_wrapper<const FieldInput>> deduplicated_nodes;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Value or Field Class
 *
 * Utility class that wraps a single value and a field, to simplify accessing both of the types.
 * \{ */

GField make_constant_field(const CPPType &type, const void *value);

template<typename T> Field<T> make_constant_field(T value)
{
  return make_constant_field(CPPType::get<T>(), &value);
}

template<typename T> struct ValueOrField {
  /** Value that is used when the field is empty. */
  T value{};
  Field<T> field;

  ValueOrField() = default;

  ValueOrField(T value) : value(std::move(value))
  {
  }

  ValueOrField(Field<T> field) : field(std::move(field))
  {
  }

  bool is_field() const
  {
    return (bool)this->field;
  }

  Field<T> as_field() const
  {
    if (this->field) {
      return this->field;
    }
    return make_constant_field(this->value);
  }

  T as_value() const
  {
    if (this->field) {
      /* This returns a default value when the field is not constant. */
      return evaluate_constant_field(this->field);
    }
    return this->value;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldNode Inline Methods
 * \{ */

inline FieldNode::FieldNode(const FieldNodeType node_type, Vector<const CPPType *> output_types)
    : node_type_(node_type), output_types_(std::move(output_types))
{
}

inline FieldNodeType FieldNode::node_type() const
{
  return node_type_;
}

inline bool FieldNode::depends_on_input() const
{
  return field_inputs_ && !field_inputs_->nodes.is_empty();
}

inline const std::shared_ptr<const FieldInputs> &FieldNode::field_inputs() const
{
  return field_inputs_;
}

inline const CPPType &FieldNode::output_cpp_type(const int output_index) const
{
  return *output_types_[output_index];
}

inline uint64_t FieldNode::hash() const
{
  return get_default_hash(this);
}

inline bool FieldNode::is_equal_to(const FieldNode &other) const
{
  return this == &other;
}

inline bool operator==(const FieldNode &a, const FieldNode &b)
{
  return a.is_equal_to(b);
}

inline bool operator!=(const FieldNode &a, const FieldNode &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldOperation Inline Methods
 * \{ */

inline Span<GField> FieldOperation::inputs() const
{
  return inputs_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldInput Inline Methods
 * \{ */

inline std::string FieldInput::socket_inspection_name() const
{
  return debug_name_;
}

inline StringRef FieldInput::debug_name() const
{
  return debug_name_;
}

inline const CPPType &FieldInput::cpp_type() const
{
  return this->output_cpp_type(0);
}

inline FieldInput::Category FieldInput::category() const
{
  return category_;
}

/** \} */

}  // namespace blender::fn
