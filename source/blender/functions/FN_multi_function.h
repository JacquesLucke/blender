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

using BLI::Vector;

class MFSignature {
 private:
  std::string m_function_name;
  Vector<std::string> m_param_names;
  Vector<MFParamType> m_param_types;
  Vector<uint> m_corrected_indices;

  friend class MultiFunction;
  friend class MFParams;

 public:
  MFSignature() = default;

  MFSignature(std::string function_name,
              Vector<std::string> param_names,
              Vector<MFParamType> param_types)
      : m_function_name(std::move(function_name)),
        m_param_names(std::move(param_names)),
        m_param_types(std::move(param_types))
  {
    uint array_or_single_refs = 0;
    uint mutable_array_refs = 0;
    uint virtual_list_list_refs = 0;
    uint vector_arrays = 0;
    for (MFParamType param_type : m_param_types) {
      uint corrected_index = 0;
      switch (param_type.type()) {
        case MFParamType::Type::SingleInput:
          corrected_index = array_or_single_refs++;
          break;
        case MFParamType::Type::SingleOutput:
          corrected_index = mutable_array_refs++;
          break;
        case MFParamType::Type::VectorInput:
          corrected_index = virtual_list_list_refs++;
          break;
        case MFParamType::Type::VectorOutput:
        case MFParamType::Type::MutableVector:
          corrected_index = vector_arrays++;
          break;
        case MFParamType::Type::MutableSingle:
          corrected_index = mutable_array_refs++;
          break;
      }
      m_corrected_indices.append(corrected_index);
    }
  }

  ArrayRef<MFParamType> param_types() const
  {
    return m_param_types;
  }

  uint get_corrected_index(uint index) const
  {
    return m_corrected_indices[index];
  }
};

class MFSignatureBuilder {
 private:
  std::string m_function_name;
  Vector<std::string> m_param_names;
  Vector<MFParamType> m_param_types;

 public:
  MFSignatureBuilder(StringRef name) : m_function_name(name)
  {
  }

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
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::Input, data_type));
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
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::Output, data_type));
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
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::Mutable, data_type));
  }

  MFSignature build()
  {
    return MFSignature(
        std::move(m_function_name), std::move(m_param_names), std::move(m_param_types));
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
    return IndexRange(m_signature.m_param_types.size());
  }

  MFParamType param_type(uint index) const
  {
    return m_signature.m_param_types[index];
  }

  StringRefNull param_name(uint index) const
  {
    return m_signature.m_param_names[index];
  }

  StringRefNull name() const
  {
    return m_signature.m_function_name;
  }

 protected:
  void set_signature(MFSignatureBuilder &signature_builder)
  {
    m_signature = signature_builder.build();
  }

 private:
  MFSignature m_signature;

  friend class MFParamsBuilder;
};

class MFParamsBuilder {
 private:
  Vector<GenericVirtualListRef> m_virtual_list_refs;
  Vector<GenericMutableArrayRef> m_mutable_array_refs;
  Vector<GenericVirtualListListRef> m_virtual_list_list_refs;
  Vector<GenericVectorArray *> m_vector_arrays;
  const MFSignature *m_signature;
  uint m_min_array_size;

  friend MFParams;

 public:
  MFParamsBuilder(const MultiFunction &function, uint min_array_size)
      : m_signature(&function.m_signature), m_min_array_size(min_array_size)
  {
  }

  template<typename T> void add_readonly_single_input(ArrayRef<T> array)
  {
    BLI_assert(array.size() >= m_min_array_size);
    m_virtual_list_refs.append(GenericVirtualListRef::FromFullArray<T>(array));
  }

  template<typename T> void add_readonly_single_input(const T *value)
  {
    m_virtual_list_refs.append(
        GenericVirtualListRef::FromSingle(CPP_TYPE<T>(), (void *)value, m_min_array_size));
  }

  void add_readonly_single_input(GenericVirtualListRef list)
  {
    BLI_assert(list.size() >= m_min_array_size);
    m_virtual_list_refs.append(list);
  }

  void add_readonly_vector_input(GenericVirtualListListRef list)
  {
    BLI_assert(list.size() >= m_min_array_size);
    m_virtual_list_list_refs.append(list);
  }

  void add_single_output(GenericMutableArrayRef array)
  {
    BLI_assert(array.size() >= m_min_array_size);
    m_mutable_array_refs.append(array);
  }

  void add_vector_output(GenericVectorArray &vector_array)
  {
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  template<typename T> void add_single_output(ArrayRef<T> array)
  {
    BLI_assert(array.size() >= m_min_array_size);
    m_mutable_array_refs.append(GenericMutableArrayRef(array));
  }

  void add_mutable_vector(GenericVectorArray &vector_array)
  {
    BLI_assert(vector_array.size() >= m_min_array_size);
    m_vector_arrays.append(&vector_array);
  }

  void add_mutable_single(GenericMutableArrayRef array)
  {
    BLI_assert(array.size() >= m_min_array_size);
    m_mutable_array_refs.append(array);
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
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return m_builder->m_virtual_list_refs[corrected_index];
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
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return m_builder->m_mutable_array_refs[corrected_index];
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
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return m_builder->m_virtual_list_list_refs[corrected_index];
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
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return *m_builder->m_vector_arrays[corrected_index];
  }

  GenericMutableArrayRef mutable_single(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::MutableSingle);
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return m_builder->m_mutable_array_refs[corrected_index];
  }
  GenericVectorArray &mutable_vector(uint index, StringRef name = "")
  {
    this->assert_correct_param(index, name, MFParamType::Type::MutableVector);
    uint corrected_index = m_builder->m_signature->get_corrected_index(index);
    return *m_builder->m_vector_arrays[corrected_index];
  }

 private:
  void assert_correct_param(uint index, StringRef name, MFParamType type) const
  {
    UNUSED_VARS_NDEBUG(index, name, type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->m_param_types[index] == type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->m_param_names[index] == name);
    }
#endif
  }

  void assert_correct_param(uint index, StringRef name, MFParamType::Type type) const
  {
    UNUSED_VARS_NDEBUG(index, name, type);
#ifdef DEBUG
    BLI_assert(m_builder->m_signature->m_param_types[index].type() == type);
    if (name.size() > 0) {
      BLI_assert(m_builder->m_signature->m_param_names[index] == name);
    }
#endif
  }

  MFParamsBuilder *m_builder;
};

};  // namespace FN

#endif /* __FN_MULTI_FUNCTION_H__ */
