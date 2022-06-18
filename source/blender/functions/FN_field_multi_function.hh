/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"

namespace blender::fn {

class FieldArrayContext {
 public:
  virtual ~FieldArrayContext() = default;

  virtual GVArray get_varray_for_input(const FieldInput &field_input, IndexMask mask) const;
};

class FieldArrayInputMixin {
 public:
  virtual ~FieldArrayInputMixin() = default;

  virtual GVArray get_varray_for_context(const FieldArrayContext &context,
                                         IndexMask mask) const = 0;
};

class FieldMultiFunctionMixin {
 private:
  /**
   * The multi-function used by this node. It is optionally owned.
   * Multi-functions with mutable or vector parameters are not supported currently.
   */
  std::shared_ptr<const MultiFunction> owned_function_;
  const MultiFunction *function_;

 public:
  FieldMultiFunctionMixin(std::shared_ptr<const MultiFunction> function)
      : FieldMultiFunctionMixin(*function.get())
  {
    owned_function_ = std::move(function);
  }

  FieldMultiFunctionMixin(const MultiFunction &function) : function_(&function)
  {
  }

  const MultiFunction &multi_function() const
  {
    return *function_;
  }
};

class FieldMultiFunctionOperation : public FieldOperation, public FieldMultiFunctionMixin {
  static Vector<const CPPType *> get_output_cpp_types(const MultiFunction &fn)
  {
    Vector<const CPPType *> types;
    for (const int param_index : fn.param_indices()) {
      const MFParamType &param_type = fn.param_type(param_index);
      if (param_type.category() == MFParamCategory::SingleOutput) {
        types.append(&param_type.data_type().single_type());
      }
    }
    return types;
  }

 public:
  FieldMultiFunctionOperation(std::shared_ptr<const MultiFunction> function, Vector<GField> inputs)
      : FieldOperation(std::move(inputs), get_output_cpp_types(*function)),
        FieldMultiFunctionMixin(std::move(function))
  {
  }

  FieldMultiFunctionOperation(const MultiFunction &function, Vector<GField> inputs)
      : FieldOperation(std::move(inputs), get_output_cpp_types(function)),
        FieldMultiFunctionMixin(function)
  {
  }
};

/**
 * Utility class that makes it easier to evaluate fields.
 */
class FieldEvaluator : NonMovable, NonCopyable {
 private:
  struct OutputPointerInfo {
    void *dst = nullptr;
    /* When a destination virtual array is provided for an input, this is
     * unnecessary, otherwise this is used to construct the required virtual array. */
    void (*set)(void *dst, const GVArray &varray, ResourceScope &scope) = nullptr;
  };

  ResourceScope scope_;
  const FieldArrayContext &context_;
  const IndexMask mask_;
  Vector<GField> fields_to_evaluate_;
  Vector<GVMutableArray> dst_varrays_;
  Vector<GVArray> evaluated_varrays_;
  Vector<OutputPointerInfo> output_pointer_infos_;
  bool is_evaluated_ = false;

  Field<bool> selection_field_;
  IndexMask selection_mask_;

 public:
  /** Takes #mask by pointer because the mask has to live longer than the evaluator. */
  FieldEvaluator(const FieldArrayContext &context, const IndexMask *mask)
      : context_(context), mask_(*mask)
  {
  }

  /** Construct a field evaluator for all indices less than #size. */
  FieldEvaluator(const FieldArrayContext &context, const int64_t size)
      : context_(context), mask_(size)
  {
  }

  ~FieldEvaluator()
  {
    /* While this assert isn't strictly necessary, and could be replaced with a warning,
     * it will catch cases where someone forgets to call #evaluate(). */
    BLI_assert(is_evaluated_);
  }

  /**
   * The selection field is evaluated first to determine which indices of the other fields should
   * be evaluated. Calling this method multiple times will just replace the previously set
   * selection field. Only the elements selected by both this selection and the selection provided
   * in the constructor are calculated. If no selection field is set, it is assumed that all
   * indices passed to the constructor are selected.
   */
  void set_selection(Field<bool> selection)
  {
    selection_field_ = std::move(selection);
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable virtual array that the evaluated result for this field is be written into.
   */
  int add_with_destination(GField field, GVMutableArray dst);

  /** Same as #add_with_destination but typed. */
  template<typename T> int add_with_destination(Field<T> field, VMutableArray<T> dst)
  {
    return this->add_with_destination(GField(std::move(field)), GVMutableArray(std::move(dst)));
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  int add_with_destination(GField field, GMutableSpan dst);

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  template<typename T> int add_with_destination(Field<T> field, MutableSpan<T> dst)
  {
    return this->add_with_destination(std::move(field), VMutableArray<T>::ForSpan(dst));
  }

  int add(GField field, GVArray *varray_ptr);

  /**
   * \param field: Field to add to the evaluator.
   * \param varray_ptr: Once #evaluate is called, the resulting virtual array will be will be
   *   assigned to the given position.
   * \return Index of the field in the evaluator which can be used in the #get_evaluated methods.
   */
  template<typename T> int add(Field<T> field, VArray<T> *varray_ptr)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_varrays_.append({});
    output_pointer_infos_.append(OutputPointerInfo{
        varray_ptr, [](void *dst, const GVArray &varray, ResourceScope &UNUSED(scope)) {
          *(VArray<T> *)dst = varray.typed<T>();
        }});
    return field_index;
  }

  /**
   * \return Index of the field in the evaluator which can be used in the #get_evaluated methods.
   */
  int add(GField field);

  /**
   * Evaluate all fields on the evaluator. This can only be called once.
   */
  void evaluate();

  const GVArray &get_evaluated(const int field_index) const
  {
    BLI_assert(is_evaluated_);
    return evaluated_varrays_[field_index];
  }

  template<typename T> VArray<T> get_evaluated(const int field_index)
  {
    return this->get_evaluated(field_index).typed<T>();
  }

  IndexMask get_evaluated_selection_as_mask();

  /**
   * Retrieve the output of an evaluated boolean field and convert it to a mask, which can be used
   * to avoid calculations for unnecessary elements later on. The evaluator will own the indices in
   * some cases, so it must live at least as long as the returned mask.
   */
  IndexMask get_evaluated_as_mask(int field_index);
};

/**
 * Evaluate fields in the given context. If possible, multiple fields should be evaluated together,
 * because that can be more efficient when they share common sub-fields.
 *
 * \param scope: The resource scope that owns data that makes up the output virtual arrays. Make
 *   sure the scope is not destructed when the output virtual arrays are still used.
 * \param fields_to_evaluate: The fields that should be evaluated together.
 * \param mask: Determines which indices are computed. The mask may be referenced by the returned
 *   virtual arrays. So the underlying indices (if applicable) should live longer then #scope.
 * \param context: The context that the field is evaluated in. Used to retrieve data from each
 *   #FieldInput in the field network.
 * \param dst_varrays: If provided, the computed data will be written into those virtual arrays
 *   instead of into newly created ones. That allows making the computed data live longer than
 *   #scope and is more efficient when the data will be written into those virtual arrays
 *   later anyway.
 * \return The computed virtual arrays for each provided field. If #dst_varrays is passed, the
 *   provided virtual arrays are returned.
 */
Vector<GVArray> evaluate_fields(ResourceScope &scope,
                                Span<GFieldRef> fields_to_evaluate,
                                IndexMask mask,
                                const FieldArrayContext &context,
                                Span<GVMutableArray> dst_varrays = {});

/* -------------------------------------------------------------------- */
/** \name Utility functions for simple field creation and evaluation
 * \{ */

void evaluate_constant_field(const GField &field, void *r_value);

template<typename T> T evaluate_constant_field(const Field<T> &field)
{
  T value;
  value.~T();
  evaluate_constant_field(field, &value);
  return value;
}

Field<bool> invert_boolean_field(const Field<bool> &field);

class IndexFieldInput final : public FieldInput, public FieldArrayInputMixin {
 public:
  IndexFieldInput();

  static GVArray get_index_varray(IndexMask mask);

  GVArray get_varray_for_context(const FieldArrayContext &context, IndexMask mask) const final;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

}  // namespace blender::fn
