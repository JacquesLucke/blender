/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "FN_multi_function_procedure_executor.hh"

#include "BLI_stack.hh"

namespace blender::fn {

MFProcedureExecutor::MFProcedureExecutor(std::string name, const MFProcedure &procedure)
    : procedure_(procedure)
{
  MFSignatureBuilder signature(std::move(name));

  for (const std::pair<MFParamType::InterfaceType, const MFVariable *> &param :
       procedure.params()) {
    signature.add(param.second->name(), MFParamType(param.first, param.second->data_type()));
  }

  signature_ = signature.build();
  this->set_signature(&signature_);
}

using IndicesSplitVectors = std::array<Vector<int64_t>, 2>;

namespace {
enum class ValueType {
  GVArray = 0,
  Span = 1,
  GVVectorArray = 2,
  GVectorArray = 3,
  OneSingle = 4,
  OneVector = 5,
};
constexpr int tot_variable_value_types = 6;
}  // namespace

struct VariableValue {
  ValueType type;

  VariableValue(ValueType type) : type(type)
  {
  }
};

/* This variable is the unmodified virtual array from the caller. */
struct VariableValue_GVArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVArray;
  const GVArray &data;

  VariableValue_GVArray(const GVArray &data) : VariableValue(static_type), data(data)
  {
  }
};

/* This variable has a different value for every index. Some values may be uninitialized. The span
 * may be owned by the caller. */
struct VariableValue_Span : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::Span;
  void *data;
  bool owned;

  VariableValue_Span(void *data, bool owned) : VariableValue(static_type), data(data), owned(owned)
  {
  }
};

/* This variable is the unmodified virtual vector array from the caller. */
struct VariableValue_GVVectorArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVVectorArray;
  const GVVectorArray &data;

  VariableValue_GVVectorArray(const GVVectorArray &data) : VariableValue(static_type), data(data)
  {
  }
};

/* This variable has a different vector for every index. */
struct VariableValue_GVectorArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVectorArray;
  GVectorArray &data;
  bool owned;

  VariableValue_GVectorArray(GVectorArray &data, bool owned)
      : VariableValue(static_type), data(data), owned(owned)
  {
  }
};

/* This variable has the same value for every index. */
struct VariableValue_OneSingle : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::OneSingle;
  void *data;
  bool is_initialized = false;

  VariableValue_OneSingle(void *data) : VariableValue(static_type), data(data)
  {
  }
};

/* This variable has the same vector for every index. */
struct VariableValue_OneVector : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::OneVector;
  GVectorArray &data;

  VariableValue_OneVector(GVectorArray &data) : VariableValue(static_type), data(data)
  {
  }
};

static_assert(std::is_trivially_destructible_v<VariableValue_GVArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_Span>);
static_assert(std::is_trivially_destructible_v<VariableValue_GVVectorArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_GVectorArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_OneSingle>);
static_assert(std::is_trivially_destructible_v<VariableValue_OneVector>);

class VariableState;

class ValueAllocator : NonCopyable, NonMovable {
 private:
  std::array<Stack<VariableValue *>, tot_variable_value_types> values_free_lists_;

 public:
  ValueAllocator() = default;

  ~ValueAllocator()
  {
    for (Stack<VariableValue *> &stack : values_free_lists_) {
      while (!stack.is_empty()) {
        MEM_freeN(stack.pop());
      }
    }
  }

  VariableState *obtain_variable_state(VariableValue &value, int tot_initialized);

  void release_variable_state(VariableState *state);

  VariableValue_GVArray *obtain_GVArray(const GVArray &varray)
  {
    return this->obtain<VariableValue_GVArray>(varray);
  }

  VariableValue_GVVectorArray *obtain_GVVectorArray(const GVVectorArray &varray)
  {
    return this->obtain<VariableValue_GVVectorArray>(varray);
  }

  VariableValue_Span *obtain_Span_not_owned(void *buffer)
  {
    return this->obtain<VariableValue_Span>(buffer, false);
  }

  VariableValue_Span *obtain_Span(const CPPType &type, int size)
  {
    void *buffer = MEM_mallocN_aligned(type.size() * size, type.alignment(), __func__);
    return this->obtain<VariableValue_Span>(buffer, true);
  }

  VariableValue_GVectorArray *obtain_GVectorArray_not_owned(GVectorArray &data)
  {
    return this->obtain<VariableValue_GVectorArray>(data, false);
  }

  VariableValue_GVectorArray *obtain_GVectorArray(const CPPType &type, int size)
  {
    GVectorArray *vector_array = new GVectorArray(type, size);
    return this->obtain<VariableValue_GVectorArray>(*vector_array, true);
  }

  VariableValue_OneSingle *obtain_OneSingle(const CPPType &type)
  {
    void *buffer = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    return this->obtain<VariableValue_OneSingle>(buffer);
  }

  VariableValue_OneVector *obtain_OneVector(const CPPType &type)
  {
    GVectorArray *vector_array = new GVectorArray(type, 1);
    return this->obtain<VariableValue_OneVector>(*vector_array);
  }

  void release_value(VariableValue *value, const MFDataType &data_type)
  {
    switch (value->type) {
      case ValueType::GVArray: {
        break;
      }
      case ValueType::Span: {
        auto *value_typed = static_cast<VariableValue_Span *>(value);
        if (value_typed->owned) {
          /* Assumes all values in the buffer are uninitialized already. */
          MEM_freeN(value_typed->data);
        }
        break;
      }
      case ValueType::GVVectorArray: {
        break;
      }
      case ValueType::GVectorArray: {
        auto *value_typed = static_cast<VariableValue_GVectorArray *>(value);
        if (value_typed->owned) {
          delete &value_typed->data;
        }
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = static_cast<VariableValue_OneSingle *>(value);
        if (value_typed->is_initialized) {
          const CPPType &type = data_type.single_type();
          type.destruct(value_typed->data);
        }
        MEM_freeN(value_typed->data);
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = static_cast<VariableValue_OneVector *>(value);
        delete &value_typed->data;
        break;
      }
    }

    Stack<VariableValue *> &stack = values_free_lists_[(int)value->type];
    stack.push(value);
  }

 private:
  template<typename T, typename... Args> T *obtain(Args &&...args)
  {
    static_assert(std::is_base_of_v<VariableValue, T>);
    Stack<VariableValue *> &stack = values_free_lists_[(int)T::static_type];
    if (stack.is_empty()) {
      void *buffer = MEM_mallocN(sizeof(T), __func__);
      return new (buffer) T(std::forward<Args>(args)...);
    }
    return new (stack.pop()) T(std::forward<Args>(args)...);
  }
};

class VariableState : NonCopyable, NonMovable {
 private:
  VariableValue *value_;
  int tot_initialized_;

 public:
  VariableState(VariableValue &value, int tot_initialized)
      : value_(&value), tot_initialized_(tot_initialized)
  {
  }

  void destruct_self(ValueAllocator &value_allocator, const MFDataType &data_type)
  {
    value_allocator.release_value(value_, data_type);
    value_allocator.release_variable_state(this);
  }

  /* True if this contains only one value for all indices, i.e. the value for all indices is
   * the same. */
  bool is_one() const
  {
    switch (value_->type) {
      case ValueType::GVArray:
        return this->value_as<VariableValue_GVArray>()->data.is_single();
      case ValueType::Span:
        return false;
      case ValueType::GVVectorArray:
        return this->value_as<VariableValue_GVVectorArray>()->data.is_single_vector();
      case ValueType::GVectorArray:
        return false;
      case ValueType::OneSingle:
        return true;
      case ValueType::OneVector:
        return true;
    }
    BLI_assert_unreachable();
    return false;
  }

  void add_as_input(MFParamsBuilder &params, IndexMask mask, const MFDataType &data_type) const
  {
    /* Sanity check to make sure that enough values are initialized. */
    BLI_assert(mask.size() <= tot_initialized_);

    switch (value_->type) {
      case ValueType::GVArray: {
        params.add_readonly_single_input(this->value_as<VariableValue_GVArray>()->data);
        break;
      }
      case ValueType::Span: {
        const void *data = this->value_as<VariableValue_Span>()->data;
        const GSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_readonly_single_input(span);
        break;
      }
      case ValueType::GVVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVVectorArray>()->data);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::OneSingle: {
        const auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        const GPointer gpointer{data_type.single_type(), value_typed->data};
        params.add_readonly_single_input(gpointer);
        break;
      }
      case ValueType::OneVector: {
        params.add_readonly_vector_input(this->value_as<VariableValue_OneVector>()->data[0]);
        break;
      }
    }
  }

  /* TODO: What happens when the same parameter is passed to the same function more than ones.
   * Maybe restrict this so that one variable can only be used either as multiple inputs, one
   * mutable or one output. */
  void ensure_is_mutable(IndexMask full_mask,
                         const MFDataType &data_type,
                         ValueAllocator &value_allocator)
  {
    if (ELEM(value_->type, ValueType::Span, ValueType::GVectorArray)) {
      return;
    }

    const int array_size = full_mask.min_array_size();

    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single_type();
        VariableValue_Span *new_value = value_allocator.obtain_Span(type, array_size);
        if (value_->type == ValueType::GVArray) {
          /* Fill new buffer with data from virtual array. */
          this->value_as<VariableValue_GVArray>()->data.materialize_to_uninitialized(
              full_mask, new_value->data);
        }
        else if (value_->type == ValueType::OneSingle) {
          auto *old_value_typed_ = this->value_as<VariableValue_OneSingle>();
          if (old_value_typed_->is_initialized) {
            /* Fill the buffer with a single value. */
            type.fill_construct_indices(old_value_typed_->data, new_value->data, full_mask);
          }
        }
        else {
          BLI_assert_unreachable();
        }
        value_allocator.release_value(value_, data_type);
        value_ = new_value;
        break;
      }
      case MFDataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        VariableValue_GVectorArray *new_value = value_allocator.obtain_GVectorArray(type,
                                                                                    array_size);
        if (value_->type == ValueType::GVVectorArray) {
          /* Fill new vector array with data from virtual vector array. */
          new_value->data.extend(full_mask, this->value_as<VariableValue_GVVectorArray>()->data);
        }
        else if (value_->type == ValueType::OneVector) {
          /* Fill all indices with the same value. */
          const GSpan vector = this->value_as<VariableValue_OneVector>()->data[0];
          new_value->data.extend(full_mask, GVVectorArray_For_SingleGSpan{vector, array_size});
        }
        else {
          BLI_assert_unreachable();
        }
        value_allocator.release_value(value_, data_type);
        value_ = new_value;
        break;
      }
    }
  }

  void add_as_mutable(MFParamsBuilder &params,
                      IndexMask mask,
                      IndexMask full_mask,
                      const MFDataType &data_type,
                      ValueAllocator &value_allocator)
  {
    /* Sanity check to make sure that enough values are initialized. */
    BLI_assert(mask.size() <= tot_initialized_);

    this->ensure_is_mutable(full_mask, data_type, value_allocator);

    switch (value_->type) {
      case ValueType::Span: {
        void *data = this->value_as<VariableValue_Span>()->data;
        const GMutableSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_single_mutable(span);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_vector_mutable(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::GVVectorArray:
      case ValueType::OneSingle:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void add_as_output(MFParamsBuilder &params,
                     IndexMask mask,
                     IndexMask full_mask,
                     const MFDataType &data_type,
                     ValueAllocator &value_allocator)
  {
    /* Sanity check to make sure that enough values are not initialized. */
    BLI_assert(mask.size() <= full_mask.size() - tot_initialized_);
    this->ensure_is_mutable(full_mask, data_type, value_allocator);

    switch (value_->type) {
      case ValueType::Span: {
        void *data = this->value_as<VariableValue_Span>()->data;
        const GMutableSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_uninitialized_single_output(span);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_vector_output(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::GVVectorArray:
      case ValueType::OneSingle:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }

    tot_initialized_ += mask.size();
  }

  void add_as_input__one(MFParamsBuilder &params, const MFDataType &data_type) const
  {
    BLI_assert(this->is_one());

    switch (value_->type) {
      case ValueType::GVArray: {
        params.add_readonly_single_input(this->value_as<VariableValue_GVArray>()->data);
        break;
      }
      case ValueType::GVVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVVectorArray>()->data);
        break;
      }
      case ValueType::OneSingle: {
        const auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        GPointer ptr{data_type.single_type(), value_typed->data};
        params.add_readonly_single_input(ptr);
        break;
      }
      case ValueType::OneVector: {
        params.add_readonly_vector_input(this->value_as<VariableValue_OneVector>()->data);
        break;
      }
      case ValueType::Span:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void ensure_is_mutable__one(const MFDataType &data_type, ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    if (ELEM(value_->type, ValueType::OneSingle, ValueType::OneVector)) {
      return;
    }

    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single_type();
        VariableValue_OneSingle *new_value = value_allocator.obtain_OneSingle(type);
        if (value_->type == ValueType::GVArray) {
          this->value_as<VariableValue_GVArray>()->data.get_internal_single_to_uninitialized(
              new_value->data);
          new_value->is_initialized = true;
        }
        else {
          BLI_assert_unreachable();
        }
        value_allocator.release_value(value_, data_type);
        value_ = new_value;
        break;
      }
      case MFDataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        VariableValue_OneVector *new_value = value_allocator.obtain_OneVector(type);
        if (value_->type == ValueType::GVVectorArray) {
          const GVVectorArray &old_vector_array =
              this->value_as<VariableValue_GVVectorArray>()->data;
          new_value->data.extend(IndexRange(1), old_vector_array);
        }
        else {
          BLI_assert_unreachable();
        }
        value_allocator.release_value(value_, data_type);
        value_ = new_value;
        break;
      }
    }
  }

  void add_as_mutable__one(MFParamsBuilder &params,
                           const MFDataType &data_type,
                           ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    this->ensure_is_mutable__one(data_type, value_allocator);

    switch (value_->type) {
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        params.add_single_mutable(GMutableSpan{data_type.single_type(), value_typed->data, 1});
        break;
      }
      case ValueType::OneVector: {
        params.add_vector_mutable(this->value_as<VariableValue_OneVector>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::Span:
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void add_as_output__one(MFParamsBuilder &params,
                          IndexMask mask,
                          const MFDataType &data_type,
                          ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    this->ensure_is_mutable__one(data_type, value_allocator);

    switch (value_->type) {
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(!value_typed->is_initialized);
        params.add_uninitialized_single_output(
            GMutableSpan{data_type.single_type(), value_typed->data, 1});
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = this->value_as<VariableValue_OneVector>();
        BLI_assert(value_typed->data[0].is_empty());
        params.add_vector_output(this->value_as<VariableValue_OneVector>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::Span:
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }

    tot_initialized_ += mask.size();
  }

  void destruct(IndexMask mask,
                IndexMask full_mask,
                const MFDataType &data_type,
                ValueAllocator &value_allocator)
  {
    /* Sanity check to make sure that enough indices can be destructed. */
    BLI_assert(tot_initialized_ >= mask.size());

    switch (value_->type) {
      case ValueType::GVArray: {
        if (mask.size() == full_mask.size()) {
          /* All elements are destructed. The elements are owned by the caller, so we don't
           * actually destruct them. */
          value_allocator.release_value(value_, data_type);
          value_ = value_allocator.obtain_OneSingle(data_type.single_type());
        }
        else {
          /* Not all elements are destructed. Since we can't work on the original array, we have to
           * create a copy first. */
          this->ensure_is_mutable(full_mask, data_type, value_allocator);
          BLI_assert(value_->type == ValueType::Span);
          const CPPType &type = data_type.single_type();
          type.destruct_indices(this->value_as<VariableValue_Span>()->data, mask);
        }
        break;
      }
      case ValueType::Span: {
        const CPPType &type = data_type.single_type();
        type.destruct_indices(this->value_as<VariableValue_Span>()->data, mask);
        break;
      }
      case ValueType::GVVectorArray: {
        if (mask.size() == full_mask.size()) {
          /* All elements are cleared. The elements are owned by the caller, so don't actually
           * destruct them. */
          value_allocator.release_value(value_, data_type);
          value_ = value_allocator.obtain_OneVector(data_type.vector_base_type());
        }
        else {
          /* Not all elements are cleared. Since we can't work on the original vector array, we
           * have to create a copy first. A possible future optimization is to create the partial
           * copy directly. */
          this->ensure_is_mutable(full_mask, data_type, value_allocator);
          BLI_assert(value_->type == ValueType::GVectorArray);
          this->value_as<VariableValue_GVectorArray>()->data.clear(mask);
        }
        break;
      }
      case ValueType::GVectorArray: {
        this->value_as<VariableValue_GVectorArray>()->data.clear(mask);
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        if (mask.size() == tot_initialized_) {
          const CPPType &type = data_type.single_type();
          type.destruct(value_typed->data);
          value_typed->is_initialized = false;
        }
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = this->value_as<VariableValue_OneVector>();
        if (mask.size() == tot_initialized_) {
          value_typed->data.clear({0});
        }
        break;
      }
    }

    tot_initialized_ -= mask.size();
  }

  void indices_split(IndexMask mask, IndicesSplitVectors &r_indices)
  {
    BLI_assert(mask.size() <= tot_initialized_);

    switch (value_->type) {
      case ValueType::GVArray: {
        const GVArray_Typed<bool> varray{this->value_as<VariableValue_GVArray>()->data};
        for (const int i : mask) {
          r_indices[varray[i]].append(i);
        }
        break;
      }
      case ValueType::Span: {
        const Span<bool> span((bool *)this->value_as<VariableValue_Span>()->data,
                              mask.min_array_size());
        for (const int i : mask) {
          r_indices[span[i]].append(i);
        }
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        const bool condition = *(bool *)value_typed->data;
        r_indices[condition].extend(mask);
        break;
      }
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  template<typename T> T *value_as()
  {
    BLI_assert(value_->type == T::static_type);
    return static_cast<T *>(value_);
  }

  template<typename T> const T *value_as() const
  {
    BLI_assert(value_->type == T::static_type);
    return static_cast<T *>(value_);
  }
};

VariableState *ValueAllocator::obtain_variable_state(VariableValue &value, int tot_initialized)
{
  return new VariableState(value, tot_initialized);
}

void ValueAllocator::release_variable_state(VariableState *state)
{
  delete state;
}

class VariableStoreContainer {
 private:
  ValueAllocator value_allocator_;
  Map<const MFVariable *, VariableState *> variable_states_;
  IndexMask full_mask_;

 public:
  VariableStoreContainer(const MFProcedureExecutor &fn,
                         const MFProcedure &procedure,
                         IndexMask full_mask,
                         MFParams &params)
      : full_mask_(full_mask)
  {

    for (const int param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      const MFVariable *variable = procedure.params()[param_index].second;

      auto add_state = [&](VariableValue *value, bool input_is_initialized) {
        const int tot_initialized = input_is_initialized ? full_mask.size() : 0;
        variable_states_.add_new(variable,
                                 value_allocator_.obtain_variable_state(*value, tot_initialized));
      };

      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &data = params.readonly_single_input(param_index);
          add_state(value_allocator_.obtain_GVArray(data), true);
          break;
        }
        case MFParamType::VectorInput: {
          const GVVectorArray &data = params.readonly_vector_input(param_index);
          add_state(value_allocator_.obtain_GVVectorArray(data), true);
          break;
        }
        case MFParamType::SingleOutput: {
          GMutableSpan data = params.uninitialized_single_output(param_index);
          add_state(value_allocator_.obtain_Span_not_owned(data.data()), false);
          break;
        }
        case MFParamType::VectorOutput: {
          GVectorArray &data = params.vector_output(param_index);
          add_state(value_allocator_.obtain_GVectorArray_not_owned(data), false);
          break;
        }
        case MFParamType::SingleMutable: {
          GMutableSpan data = params.single_mutable(param_index);
          add_state(value_allocator_.obtain_Span_not_owned(data.data()), true);
          break;
        }
        case MFParamType::VectorMutable: {
          GVectorArray &data = params.vector_mutable(param_index);
          add_state(value_allocator_.obtain_GVectorArray_not_owned(data), true);
          break;
        }
      }
    }
  }

  ~VariableStoreContainer()
  {
    for (auto &&item : variable_states_.items()) {
      const MFVariable *variable = item.key;
      VariableState *state = item.value;
      state->destruct_self(value_allocator_, variable->data_type());
    }
  }

  void load_param(MFParamsBuilder &params,
                  const MFVariable &variable,
                  const MFParamType &param_type,
                  const IndexMask &mask)
  {
    VariableState &variable_state = this->get_variable_state(variable);
    switch (param_type.interface_type()) {
      case MFParamType::Input: {
        variable_state.add_as_input(params, mask, variable.data_type());
        break;
      }
      case MFParamType::Mutable: {
        variable_state.add_as_mutable(
            params, mask, full_mask_, variable.data_type(), value_allocator_);
        break;
      }
      case MFParamType::Output: {
        variable_state.add_as_output(
            params, mask, full_mask_, variable.data_type(), value_allocator_);
        break;
      }
    }
  }

  void destruct(const MFVariable &variable, const IndexMask &mask)
  {
    VariableState &variable_state = this->get_variable_state(variable);
    variable_state.destruct(mask, full_mask_, variable.data_type(), value_allocator_);
  }

  VariableState &get_variable_state(const MFVariable &variable)
  {
    return *variable_states_.lookup_or_add_cb(
        &variable, [&]() { return this->create_new_state_for_variable(variable); });
  }

  VariableState *create_new_state_for_variable(const MFVariable &variable)
  {
    MFDataType data_type = variable.data_type();
    switch (data_type.category()) {
      case MFDataType::Single: {
        const CPPType &type = data_type.single_type();
        return value_allocator_.obtain_variable_state(*value_allocator_.obtain_OneSingle(type), 0);
      }
      case MFDataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        return value_allocator_.obtain_variable_state(*value_allocator_.obtain_OneVector(type), 0);
      }
    }
    BLI_assert_unreachable();
    return nullptr;
  }
};

static void execute_call_instruction(const MFCallInstruction &instruction,
                                     IndexMask mask,
                                     VariableStoreContainer &variable_stores,
                                     const MFContext &context)
{
  const MultiFunction &fn = instruction.fn();
  MFParamsBuilder params(fn, mask.min_array_size());

  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    const MFVariable *variable = instruction.params()[param_index];
    variable_stores.load_param(params, *variable, param_type, mask);
  }

  fn.call(mask, params, context);
}

struct NextInstructionInfo {
  const MFInstruction *instruction = nullptr;
  Vector<int64_t> owned_indices;

  IndexMask mask() const
  {
    return this->owned_indices.as_span();
  }

  operator bool() const
  {
    return this->instruction != nullptr;
  }
};

class InstructionScheduler {
 private:
  Map<const MFInstruction *, Vector<Vector<int64_t>>> indices_by_instruction_;

 public:
  InstructionScheduler() = default;

  void add_referenced_indices(const MFInstruction *instruction, IndexMask mask)
  {
    if (instruction == nullptr) {
      return;
    }
    if (mask.is_empty()) {
      return;
    }
    indices_by_instruction_.lookup_or_add_default(instruction).append(mask.indices());
  }

  void add_owned_indices(const MFInstruction *instruction, Vector<int64_t> indices)
  {
    if (instruction == nullptr) {
      return;
    }
    if (indices.is_empty()) {
      return;
    }
    BLI_assert(IndexMask::indices_are_valid_index_mask(indices));
    indices_by_instruction_.lookup_or_add_default(instruction).append(std::move(indices));
  }

  void add_previous_instruction_indices(const MFInstruction *instruction,
                                        NextInstructionInfo &instr_info)
  {
    this->add_owned_indices(instruction, std::move(instr_info.owned_indices));
  }

  NextInstructionInfo pop_next()
  {
    if (indices_by_instruction_.is_empty()) {
      return {};
    }
    /* TODO: Implement better mechanism to determine next instruction. */
    const MFInstruction *instruction = *indices_by_instruction_.keys().begin();

    Vector<int64_t> indices = this->pop_indices_array(instruction);
    if (indices.is_empty()) {
      return {};
    }

    NextInstructionInfo next_instruction_info;
    next_instruction_info.instruction = instruction;
    next_instruction_info.owned_indices = std::move(indices);
    return next_instruction_info;
  }

 private:
  Vector<int64_t> pop_indices_array(const MFInstruction *instruction)
  {
    Vector<Vector<int64_t>> *indices = indices_by_instruction_.lookup_ptr(instruction);
    if (indices == nullptr) {
      return {};
    }
    Vector<int64_t> r_indices;
    while (!indices->is_empty() && r_indices.is_empty()) {
      r_indices = (*indices).pop_last();
    }
    if (indices->is_empty()) {
      indices_by_instruction_.remove_contained(instruction);
    }
    return r_indices;
  }
};

void MFProcedureExecutor::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (procedure_.entry() == nullptr) {
    return;
  }

  LinearAllocator<> allocator;

  VariableStoreContainer variable_stores{*this, procedure_, mask, params};

  InstructionScheduler scheduler;
  scheduler.add_referenced_indices(procedure_.entry(), mask);

  while (NextInstructionInfo instr_info = scheduler.pop_next()) {
    const MFInstruction &instruction = *instr_info.instruction;
    switch (instruction.type()) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instruction = static_cast<const MFCallInstruction &>(
            instruction);
        execute_call_instruction(call_instruction, instr_info.mask(), variable_stores, context);
        scheduler.add_previous_instruction_indices(call_instruction.next(), instr_info);
        break;
      }
      case MFInstructionType::Branch: {
        const MFBranchInstruction &branch_instruction = static_cast<const MFBranchInstruction &>(
            instruction);
        const MFVariable *condition_var = branch_instruction.condition();
        VariableState &variable_state = variable_stores.get_variable_state(*condition_var);

        IndicesSplitVectors new_indices;
        variable_state.indices_split(instr_info.mask(), new_indices);
        scheduler.add_owned_indices(branch_instruction.branch_false(), new_indices[false]);
        scheduler.add_owned_indices(branch_instruction.branch_true(), new_indices[true]);
        break;
      }
      case MFInstructionType::Destruct: {
        const MFDestructInstruction &destruct_instruction =
            static_cast<const MFDestructInstruction &>(instruction);
        const MFVariable *variable = destruct_instruction.variable();
        variable_stores.destruct(*variable, instr_info.mask());
        scheduler.add_previous_instruction_indices(destruct_instruction.next(), instr_info);
        break;
      }
    }

    /* TODO: Make sure outputs are copied into the right place. */
  }
}

}  // namespace blender::fn
