#ifndef __FN_MULTI_FUNCTION_H__
#define __FN_MULTI_FUNCTION_H__

#include "FN_generic_array_ref.h"
#include "FN_generic_vector_array.h"
#include "FN_generic_virtual_list_ref.h"
#include "FN_generic_virtual_list_list_ref.h"
#include "FN_multi_function_data_type.h"
#include "FN_multi_function_param_type.h"
#include "FN_multi_function_mask.h"
#include "FN_multi_function_context.h"

#include "BLI_vector.h"

namespace FN {

class MultiFunction;

struct MFSignatureData {
  std::string function_name;
  Vector<std::string> param_names;
  Vector<MFParamType> param_types;
  Vector<BLI::class_id_t> used_element_contexts;
  Vector<BLI::class_id_t> used_global_contexts;
  Vector<uint> param_data_indices;

  uint data_index(uint param_index) const
  {
    return this->param_data_indices[param_index];
  }
};

class MFSignatureBuilder {
 private:
  MFSignatureData &m_data;
  uint m_array_ref_count = 0;
  uint m_virtual_list_count = 0;
  uint m_virtual_list_list_count = 0;
  uint m_vector_array_count = 0;

 public:
  MFSignatureBuilder(MFSignatureData &data) : m_data(data)
  {
  }

  /* Used Contexts */

  template<typename T> void use_element_context()
  {
    BLI::class_id_t id = BLI::get_class_id<T>();
    m_data.used_element_contexts.append_non_duplicates(id);
  }

  template<typename T> void use_global_context()
  {
    BLI::class_id_t id = BLI::get_class_id<T>();
    m_data.used_global_contexts.append_non_duplicates(id);
  }

  void copy_used_contexts(const MultiFunction &fn);

  /* Input Param Types */

  template<typename T> void single_input(StringRef name)
  {
    this->single_input(name, CPP_TYPE<T>());
  }
  void single_input(StringRef name, const CPPType &type)
  {
    this->input(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_input(StringRef name)
  {
    this->vector_input(name, CPP_TYPE<T>());
  }
  void vector_input(StringRef name, const CPPType &base_type)
  {
    this->input(name, MFDataType::ForVector(base_type));
  }
  void input(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Input, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_virtual_list_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_virtual_list_list_count++);
        break;
    }
  }

  /* Output Param Types */

  template<typename T> void single_output(StringRef name)
  {
    this->single_output(name, CPP_TYPE<T>());
  }
  void single_output(StringRef name, const CPPType &type)
  {
    this->output(name, MFDataType::ForSingle(type));
  }
  template<typename T> void vector_output(StringRef name)
  {
    this->vector_output(name, CPP_TYPE<T>());
  }
  void vector_output(StringRef name, const CPPType &base_type)
  {
    this->output(name, MFDataType::ForVector(base_type));
  }
  void output(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Output, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_array_ref_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_vector_array_count++);
        break;
    }
  }

  /* Mutable Param Types */

  void mutable_single(StringRef name, const CPPType &type)
  {
    this->mutable_param(name, MFDataType::ForSingle(type));
  }
  void mutable_vector(StringRef name, const CPPType &base_type)
  {
    this->mutable_param(name, MFDataType::ForVector(base_type));
  }
  void mutable_param(StringRef name, MFDataType data_type)
  {
    m_data.param_names.append(name);
    m_data.param_types.append(MFParamType(MFParamType::Mutable, data_type));

    switch (data_type.category()) {
      case MFDataType::Single:
        m_data.param_data_indices.append(m_array_ref_count++);
        break;
      case MFDataType::Vector:
        m_data.param_data_indices.append(m_vector_array_count++);
        break;
    }
  }
};

class MFParams;

class MultiFunction {
 public:
  virtual ~MultiFunction()
  {
  }
  virtual void call(MFMask mask, MFParams params, MFContext context) const = 0;

  IndexRange param_indices() const
  {
    return IndexRange(m_signature_data.param_types.size());
  }

  MFParamType param_type(uint index) const
  {
    return m_signature_data.param_types[index];
  }

  StringRefNull param_name(uint index) const
  {
    return m_signature_data.param_names[index];
  }

  StringRefNull name() const
  {
    return m_signature_data.function_name;
  }

  bool depends_on_per_element_context() const
  {
    return m_signature_data.used_element_contexts.size() > 0;
  }

  template<typename T> bool uses_element_context() const
  {
    BLI::class_id_t id = BLI::get_class_id<T>();
    return m_signature_data.used_element_contexts.contains(id);
  }

  template<typename T> bool uses_global_context() const
  {
    BLI::class_id_t id = BLI::get_class_id<T>();
    return m_signature_data.used_global_contexts.contains(id);
  }

 protected:
  MFSignatureBuilder get_builder(StringRef function_name)
  {
    m_signature_data.function_name = function_name;
    return MFSignatureBuilder(m_signature_data);
  }

 private:
  MFSignatureData m_signature_data;

  friend class MFParamsBuilder;
  friend class MFSignatureBuilder;
};

class MFParamsBuilder {
 private:
  Vector<GenericVirtualListRef> m_virtual_list_refs;
  Vector<GenericMutableArrayRef> m_mutable_array_refs;
  Vector<GenericVirtualListListRef> m_virtual_list_list_refs;
  Vector<GenericVectorArray *> m_vector_arrays;
  const MFSignatureData *m_signature;
  uint m_min_array_size;

  friend MFParams;

 public:
  MFParamsBuilder(const MultiFunction &function, uint min_array_size)
      : m_signature(&function.m_signature_data), m_min_array_size(min_array_size)
  {
  }

  template<typename T> void add_readonly_single_input(ArrayRef<T> array)
  {
    this->add_readonly_single_input(GenericVirtualListRef::FromFullArray<T>(array));
  }

  template<typename T> void add_readonly_single_input(const T *value)
  {
    this->add_readonly_single_input(
        GenericVirtualListRef::FromSingle(CPP_TYPE<T>(), (void *)value, m_min_array_size));
  }

  void add_readonly_single_input(GenericVirtualListRef list)
  {
    this->assert_current_param_type(MFParamType::ForSingleInput(list.type()));
    BLI_assert(list.size() >= m_min_array_size);
    m_virtual_list_refs.append(list);
  }

  void add_readonly_vector_input(GenericVirtualListListRef list)
  {
    this->assert_current_param_type(MFParamType::ForVectorInput(list.type()));
    BLI_assert(list.size() >= m_min_array_size);
    m_virtual_list_list_refs.append(list);
  }

  template<typename T> void add_single_output(MutableArrayRef<T> array)
  {
    BLI_assert(array.size() >= m_min_array_size);
    this->add_single_output(GenericMutableArrayRef(array));
  }
  void add_single_output(GenericMutableArrayRef array)
  {
    this->assert_current_param_type(MFParamType::ForSingleOutput(array.type()));
    BLI_assert(array.size() >= m_min_array_size);
    m_mutable_array_refs.append(array);
  }

  void add_vector_output(GenericVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForVectorOutput(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  void add_mutable_vector(GenericVectorArray &vector_array)
  {
    this->assert_current_param_type(MFParamType::ForVectorMutable(vector_array.type()));
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  void add_mutable_single(GenericMutableArrayRef array)
  {
    this->assert_current_param_type(MFParamType::ForSingleMutable(array.type()));
    BLI_assert(array.size() >= m_min_array_size);
    m_mutable_array_refs.append(array);
  }

  /* Utilities to get the data after the function has been called. */

  GenericMutableArrayRef computed_array(uint index)
  {
    BLI_assert(ELEM(m_signature->param_types[index].type(),
                    MFParamType::MutableSingle,
                    MFParamType::SingleOutput));
    uint data_index = m_signature->data_index(index);
    return m_mutable_array_refs[data_index];
  }

  GenericVectorArray &computed_vector_array(uint index)
  {
    BLI_assert(ELEM(m_signature->param_types[index].type(),
                    MFParamType::MutableVector,
                    MFParamType::VectorOutput));
    uint data_index = m_signature->data_index(index);
    return *m_vector_arrays[data_index];
  }

 private:
  void assert_current_param_type(MFParamType param_type) const
  {
    UNUSED_VARS_NDEBUG(param_type);
#ifdef DEBUG
    uint param_index = this->current_param_index();
    MFParamType expected_type = m_signature->param_types[param_index];
    BLI_assert(expected_type == param_type);
#endif
  }

  void assert_current_param_type(MFParamType::Type type) const
  {
    UNUSED_VARS_NDEBUG(type);
#ifdef DEBUG
    uint param_index = this->current_param_index();
    MFParamType::Type expected_type = m_signature->param_types[param_index].type();
    BLI_assert(expected_type == type);
#endif
  }

  uint current_param_index() const
  {
    return m_mutable_array_refs.size() + m_virtual_list_refs.size() +
           m_virtual_list_list_refs.size() + m_vector_arrays.size();
  }
};

class MFParams {
 public:
  MFParams(MFParamsBuilder &builder) : m_builder(&builder)
  {
  }

  template<typename T> VirtualListRef<T> readonly_single_input(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::ForSingleInput(CPP_TYPE<T>()));
    return this->readonly_single_input(index, name).as_typed_ref<T>();
  }

  GenericVirtualListRef readonly_single_input(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::SingleInput);
    uint data_index = m_builder->m_signature->data_index(index);
    return m_builder->m_virtual_list_refs[data_index];
  }

  template<typename T>
  MutableArrayRef<T> uninitialized_single_output(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::ForSingleOutput(CPP_TYPE<T>()));
    return this->uninitialized_single_output(index, name).as_typed_ref<T>();
  }
  GenericMutableArrayRef uninitialized_single_output(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::SingleOutput);
    uint data_index = m_builder->m_signature->data_index(index);
    return m_builder->m_mutable_array_refs[data_index];
  }

  template<typename T>
  const VirtualListListRef<T> readonly_vector_input(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::ForVectorInput(CPP_TYPE<T>()));
    return this->readonly_vector_input(index, name).as_typed_ref<T>();
  }
  GenericVirtualListListRef readonly_vector_input(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::VectorInput);
    uint data_index = m_builder->m_signature->data_index(index);
    return m_builder->m_virtual_list_list_refs[data_index];
  }

  template<typename T>
  GenericVectorArray::MutableTypedRef<T> vector_output(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::ForVectorOutput(CPP_TYPE<T>()));
    return this->vector_output(index, name).as_mutable_typed_ref<T>();
  }
  GenericVectorArray &vector_output(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::VectorOutput);
    uint data_index = m_builder->m_signature->data_index(index);
    return *m_builder->m_vector_arrays[data_index];
  }

  GenericMutableArrayRef mutable_single(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::MutableSingle);
    uint data_index = m_builder->m_signature->data_index(index);
    return m_builder->m_mutable_array_refs[data_index];
  }
  GenericVectorArray &mutable_vector(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::MutableVector);
    uint data_index = m_builder->m_signature->data_index(index);
    return *m_builder->m_vector_arrays[data_index];
  }

 private:
  void assert_correct_param(uint index, StringRef name, MFParamType type) const
  {
    UNUSED_VARS_NDEBUG(index, name, type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[index] == type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[index] == name);
    }
#endif
  }

  void assert_correct_param(uint index, StringRef name, MFParamType::Type type) const
  {
    UNUSED_VARS_NDEBUG(index, name, type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->param_types[index].type() == type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->param_names[index] == name);
    }
#endif
  }

  MFParamsBuilder *m_builder;
};

inline void MFSignatureBuilder::copy_used_contexts(const MultiFunction &fn)
{
  m_data.used_element_contexts.extend_non_duplicates(fn.m_signature_data.used_element_contexts);
  m_data.used_global_contexts.extend_non_duplicates(fn.m_signature_data.used_global_contexts);
}

};  // namespace FN

#endif /* __FN_MULTI_FUNCTION_H__ */
