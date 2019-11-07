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
      switch (param_type.category()) {
        case MFParamType::None:
          BLI_assert(false);
          break;
        case MFParamType::ReadonlySingleInput:
          corrected_index = array_or_single_refs++;
          break;
        case MFParamType::SingleOutput:
          corrected_index = mutable_array_refs++;
          break;
        case MFParamType::ReadonlyVectorInput:
          corrected_index = virtual_list_list_refs++;
          break;
        case MFParamType::VectorOutput:
        case MFParamType::MutableVector:
          corrected_index = vector_arrays++;
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

  template<typename T> bool is_readonly_single_input(uint index, StringRef name) const
  {
    return this->is_valid_param<T>(index, name, MFParamType::ReadonlySingleInput);
  }
  bool is_readonly_single_input(uint index, StringRef name) const
  {
    return this->is_valid_param(index, name, MFParamType::ReadonlySingleInput);
  }

  template<typename T> bool is_single_output(uint index, StringRef name) const
  {
    return this->is_valid_param<T>(index, name, MFParamType::SingleOutput);
  }
  bool is_single_output(uint index, StringRef name) const
  {
    return this->is_valid_param(index, name, MFParamType::SingleOutput);
  }

  template<typename T> bool is_readonly_vector_input(uint index, StringRef name) const
  {
    return this->is_valid_param<T>(index, name, MFParamType::ReadonlyVectorInput);
  }
  bool is_readonly_vector_input(uint index, StringRef name) const
  {
    return this->is_valid_param(index, name, MFParamType::ReadonlyVectorInput);
  }

  template<typename T> bool is_vector_output(uint index, StringRef name) const
  {
    return this->is_valid_param<T>(index, name, MFParamType::VectorOutput);
  }
  bool is_vector_output(uint index, StringRef name) const
  {
    return this->is_valid_param(index, name, MFParamType::VectorOutput);
  }

  bool is_mutable_vector(uint index, StringRef name) const
  {
    return this->is_valid_param(index, name, MFParamType::MutableVector);
  }

 private:
  template<typename T>
  bool is_valid_param(uint index, StringRef name, MFParamType::Category category) const
  {
    if (!this->is_valid_param(index, name, category)) {
      return false;
    }
    else if (ELEM(category, MFParamType::ReadonlySingleInput, MFParamType::SingleOutput)) {
      return CPP_TYPE<T>().is_same_or_generalization(m_param_types[index].type());
    }
    else if (ELEM(category,
                  MFParamType::ReadonlyVectorInput,
                  MFParamType::VectorOutput,
                  MFParamType::MutableVector)) {
      return CPP_TYPE<T>().is_same_or_generalization(m_param_types[index].base_type());
    }
    else {
      return false;
    }
  }

  bool is_valid_param(uint index, StringRef name, MFParamType::Category category) const
  {
    return m_param_names[index] == name && m_param_types[index].category() == category;
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

  template<typename T> void readonly_single_input(StringRef name)
  {
    this->readonly_single_input(name, CPP_TYPE<T>());
  }
  void readonly_single_input(StringRef name, const CPPType &type)
  {
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::ReadonlySingleInput, &type));
  }

  template<typename T> void single_output(StringRef name)
  {
    this->single_output(name, CPP_TYPE<T>());
  }
  void single_output(StringRef name, const CPPType &type)
  {
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::SingleOutput, &type));
  }

  template<typename T> void readonly_vector_input(StringRef name)
  {
    this->readonly_vector_input(name, CPP_TYPE<T>());
  }
  void readonly_vector_input(StringRef name, const CPPType &base_type)
  {
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::ReadonlyVectorInput, &base_type));
  }

  template<typename T> void vector_output(StringRef name)
  {
    this->vector_output(name, CPP_TYPE<T>());
  }
  void vector_output(StringRef name, const CPPType &base_type)
  {
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::VectorOutput, &base_type));
  }

  void mutable_vector(StringRef name, const CPPType &base_type)
  {
    m_param_names.append(name);
    m_param_types.append(MFParamType(MFParamType::MutableVector, &base_type));
  }

  MFSignature build()
  {
    return MFSignature(
        std::move(m_function_name), std::move(m_param_names), std::move(m_param_types));
  }
};

class MFParams {
 public:
  MFParams() = default;

  MFParams(ArrayRef<GenericVirtualListRef> virtual_list_refs,
           ArrayRef<GenericMutableArrayRef> mutable_array_refs,
           ArrayRef<GenericVirtualListListRef> virtual_list_list_refs,
           ArrayRef<GenericVectorArray *> vector_arrays,
           const MFSignature &signature)
      : m_virtual_list_refs(virtual_list_refs),
        m_mutable_array_refs(mutable_array_refs),
        m_virtual_list_list_refs(virtual_list_list_refs),
        m_vector_arrays(vector_arrays),
        m_signature(&signature)
  {
  }

  template<typename T> VirtualListRef<T> readonly_single_input(uint index, StringRef name)
  {
    BLI_assert(m_signature->is_readonly_single_input<T>(index, name));
    return this->readonly_single_input(index, name).as_typed_ref<T>();
  }
  GenericVirtualListRef readonly_single_input(uint index, StringRef name)
  {
    UNUSED_VARS_NDEBUG(name);
    BLI_assert(m_signature->is_readonly_single_input(index, name));
    uint corrected_index = m_signature->get_corrected_index(index);
    return m_virtual_list_refs[corrected_index];
  }

  template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name)
  {
    BLI_assert(m_signature->is_single_output<T>(index, name));
    return this->single_output(index, name).as_typed_ref<T>();
  }
  GenericMutableArrayRef single_output(uint index, StringRef name)
  {
    UNUSED_VARS_NDEBUG(name);
    BLI_assert(m_signature->is_single_output(index, name));
    uint corrected_index = m_signature->get_corrected_index(index);
    return m_mutable_array_refs[corrected_index];
  }

  template<typename T>
  const VirtualListListRef<T> readonly_vector_input(uint index, StringRef name)
  {
    BLI_assert(m_signature->is_readonly_vector_input<T>(index, name));
    return this->readonly_vector_input(index, name).as_typed_ref<T>();
  }
  GenericVirtualListListRef readonly_vector_input(uint index, StringRef name)
  {
    UNUSED_VARS_NDEBUG(name);
    BLI_assert(m_signature->is_readonly_vector_input(index, name));
    uint corrected_index = m_signature->get_corrected_index(index);
    return m_virtual_list_list_refs[corrected_index];
  }

  template<typename T>
  GenericVectorArray::MutableTypedRef<T> vector_output(uint index, StringRef name)
  {
    BLI_assert(m_signature->is_vector_output<T>(index, name));
    return this->vector_output(index, name).as_mutable_typed_ref<T>();
  }
  GenericVectorArray &vector_output(uint index, StringRef name)
  {
    UNUSED_VARS_NDEBUG(name);
    BLI_assert(m_signature->is_vector_output(index, name));
    uint corrected_index = m_signature->get_corrected_index(index);
    return *m_vector_arrays[corrected_index];
  }

  GenericVectorArray &mutable_vector(uint index, StringRef name)
  {
    UNUSED_VARS_NDEBUG(name);
    BLI_assert(m_signature->is_mutable_vector(index, name));
    uint corrected_index = m_signature->get_corrected_index(index);
    return *m_vector_arrays[corrected_index];
  }

 private:
  ArrayRef<GenericVirtualListRef> m_virtual_list_refs;
  ArrayRef<GenericMutableArrayRef> m_mutable_array_refs;
  ArrayRef<GenericVirtualListListRef> m_virtual_list_list_refs;
  ArrayRef<GenericVectorArray *> m_vector_arrays;
  const MFSignature *m_signature = nullptr;
};

class MultiFunction {
 public:
  virtual ~MultiFunction()
  {
  }
  virtual void call(const MFMask &mask, MFParams &params, MFContext &context) const = 0;

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
  const MFSignature *m_signature = nullptr;
  uint m_min_array_size;

  MFParams m_params;

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

  MFParams &build()
  {
    BLI_assert(m_signature != nullptr);
    m_params = MFParams(m_virtual_list_refs,
                        m_mutable_array_refs,
                        m_virtual_list_list_refs,
                        m_vector_arrays,
                        *m_signature);
    return m_params;
  }
};

};  // namespace FN

#endif /* __FN_MULTI_FUNCTION_H__ */
