#ifndef __BKE_MULTI_FUNCTION_H__
#define __BKE_MULTI_FUNCTION_H__

#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"

#include "BLI_vector.h"

namespace BKE {

using BLI::Vector;

class MultiFunction {
 public:
  enum ParamCategory {
    SingleInput,
    SingleOutput,
    VectorInput,
    VectorOutput,
    MutableVector,
  };

  class Signature {
   private:
    Vector<std::string> m_param_names;
    Vector<ParamCategory> m_param_categories;
    Vector<CPPType *> m_param_base_types;
    Vector<uint> m_corrected_indices;

   public:
    Signature() = default;

    Signature(Vector<std::string> param_names,
              Vector<ParamCategory> param_categories,
              Vector<CPPType *> param_base_types)
        : m_param_names(std::move(param_names)),
          m_param_categories(std::move(param_categories)),
          m_param_base_types(std::move(param_base_types))
    {
      uint array_or_single_refs = 0;
      uint mutable_array_refs = 0;
      uint vector_array_or_single_refs = 0;
      uint vector_arrays = 0;
      for (ParamCategory category : m_param_categories) {
        uint corrected_index = 0;
        switch (category) {
          case ParamCategory::SingleInput:
            corrected_index = array_or_single_refs++;
            break;
          case ParamCategory::SingleOutput:
            corrected_index = mutable_array_refs++;
            break;
          case ParamCategory::VectorInput:
            corrected_index = vector_array_or_single_refs++;
            break;
          case ParamCategory::VectorOutput:
          case ParamCategory::MutableVector:
            corrected_index = vector_arrays++;
            break;
        }
        m_corrected_indices.append(corrected_index);
      }
    }

    uint get_corrected_index(uint index) const
    {
      return m_corrected_indices[index];
    }

    template<typename T> bool is_readonly_single_input(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamCategory::SingleInput);
    }
    bool is_readonly_single_input(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamCategory::SingleInput);
    }

    template<typename T> bool is_single_output(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamCategory::SingleOutput);
    }
    bool is_single_output(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamCategory::SingleOutput);
    }

    template<typename T> bool is_readonly_vector_input(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamCategory::VectorInput);
    }
    bool is_readonly_vector_input(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamCategory::VectorInput);
    }

    template<typename T> bool is_vector_output(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamCategory::VectorOutput);
    }
    bool is_vector_output(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamCategory::VectorOutput);
    }

    bool is_mutable_vector(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamCategory::MutableVector);
    }

   private:
    template<typename T>
    bool is_valid_param(uint index, StringRef name, ParamCategory category) const
    {
      return this->is_valid_param(index, name, category) &&
             m_param_base_types[index] == &GET_TYPE<T>();
    }

    bool is_valid_param(uint index, StringRef name, ParamCategory category) const
    {
      return m_param_names[index] == name && m_param_categories[index] == category;
    }
  };

  class SignatureBuilder {
   private:
    Vector<std::string> m_param_names;
    Vector<ParamCategory> m_param_categories;
    Vector<CPPType *> m_param_base_types;

   public:
    template<typename T> void readonly_single_input(StringRef name)
    {
      this->readonly_single_input(name, GET_TYPE<T>());
    }
    void readonly_single_input(StringRef name, CPPType &type)
    {
      m_param_names.append(name);
      m_param_base_types.append(&type);
      m_param_categories.append(ParamCategory::SingleInput);
    }

    template<typename T> void single_output(StringRef name)
    {
      this->single_output(name, GET_TYPE<T>());
    }
    void single_output(StringRef name, CPPType &type)
    {
      m_param_names.append(name);
      m_param_base_types.append(&type);
      m_param_categories.append(ParamCategory::SingleOutput);
    }

    template<typename T> void readonly_vector_input(StringRef name)
    {
      this->readonly_vector_input(name, GET_TYPE<T>());
    }
    void readonly_vector_input(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_base_types.append(&base_type);
      m_param_categories.append(ParamCategory::VectorInput);
    }

    template<typename T> void vector_output(StringRef name)
    {
      this->vector_output(name, GET_TYPE<T>());
    }
    void vector_output(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_base_types.append(&base_type);
      m_param_categories.append(ParamCategory::VectorOutput);
    }

    void mutable_vector(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_base_types.append(&base_type);
      m_param_categories.append(ParamCategory::MutableVector);
    }

    Signature build()
    {
      return Signature(
          std::move(m_param_names), std::move(m_param_categories), std::move(m_param_base_types));
    }
  };

  class Params {
   public:
    Params() = default;

    Params(ArrayRef<GenericArrayOrSingleRef> array_or_single_refs,
           ArrayRef<GenericMutableArrayRef> mutable_array_refs,
           ArrayRef<GenericVectorArrayOrSingleRef> vector_array_or_single_refs,
           ArrayRef<GenericVectorArray *> vector_arrays,
           const Signature &signature)
        : m_array_or_single_refs(array_or_single_refs),
          m_mutable_array_refs(mutable_array_refs),
          m_vector_array_or_single_refs(vector_array_or_single_refs),
          m_vector_arrays(vector_arrays),
          m_signature(&signature)
    {
    }

    template<typename T> ArrayOrSingleRef<T> readonly_single_input(uint index, StringRef name)
    {
      BLI_assert(m_signature->is_readonly_single_input<T>(index, name));
      return this->readonly_single_input(index, name).as_typed_ref<T>();
    }
    GenericArrayOrSingleRef readonly_single_input(uint index, StringRef name)
    {
      UNUSED_VARS_NDEBUG(name);
      BLI_assert(m_signature->is_readonly_single_input(index, name));
      uint corrected_index = m_signature->get_corrected_index(index);
      return m_array_or_single_refs[corrected_index];
    }

    template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name)
    {
      BLI_assert(m_signature->is_single_output<T>(index, name));
      return this->single_output(index, name).get_ref<T>();
    }
    GenericMutableArrayRef single_output(uint index, StringRef name)
    {
      UNUSED_VARS_NDEBUG(name);
      BLI_assert(m_signature->is_single_output(index, name));
      uint corrected_index = m_signature->get_corrected_index(index);
      return m_mutable_array_refs[corrected_index];
    }

    template<typename T>
    const GenericVectorArrayOrSingleRef::TypedRef<T> readonly_vector_input(uint index,
                                                                           StringRef name)
    {
      BLI_assert(m_signature->is_readonly_vector_input<T>(index, name));
      return this->readonly_vector_input(index, name).as_typed_ref<T>();
    }
    GenericVectorArrayOrSingleRef readonly_vector_input(uint index, StringRef name)
    {
      UNUSED_VARS_NDEBUG(name);
      BLI_assert(m_signature->is_readonly_vector_input(index, name));
      uint corrected_index = m_signature->get_corrected_index(index);
      return m_vector_array_or_single_refs[corrected_index];
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
    ArrayRef<GenericArrayOrSingleRef> m_array_or_single_refs;
    ArrayRef<GenericMutableArrayRef> m_mutable_array_refs;
    ArrayRef<GenericVectorArrayOrSingleRef> m_vector_array_or_single_refs;
    ArrayRef<GenericVectorArray *> m_vector_arrays;
    const Signature *m_signature = nullptr;
  };

  class ParamsBuilder {
   private:
    Vector<GenericArrayOrSingleRef> m_array_or_single_refs;
    Vector<GenericMutableArrayRef> m_mutable_array_refs;
    Vector<GenericVectorArrayOrSingleRef> m_vector_array_or_single_refs;
    Vector<GenericVectorArray *> m_vector_arrays;
    const Signature *m_signature = nullptr;
    uint m_min_array_size;

    Params m_params;

   public:
    ParamsBuilder() = default;

    void start_new(const Signature &signature, uint min_array_size)
    {
      m_signature = &signature;
      m_min_array_size = min_array_size;

      m_array_or_single_refs.clear();
      m_mutable_array_refs.clear();
      m_vector_array_or_single_refs.clear();
      m_vector_arrays.clear();
    }

    template<typename T> void add_readonly_array_ref(ArrayRef<T> array)
    {
      BLI_assert(array.size() >= m_min_array_size);
      m_array_or_single_refs.append(GenericArrayOrSingleRef::FromArray<T>(array));
    }

    template<typename T> void add_readonly_single_ref(const T *value)
    {
      m_array_or_single_refs.append(
          GenericArrayOrSingleRef::FromSingle(GET_TYPE<T>(), (void *)value, m_min_array_size));
    }

    template<typename T> void add_mutable_array_ref(ArrayRef<T> array)
    {
      BLI_assert(array.size() >= m_min_array_size);
      m_mutable_array_refs.append(GenericMutableArrayRef(array));
    }

    Params &build()
    {
      BLI_assert(m_signature != nullptr);
      m_params = Params(m_array_or_single_refs,
                        m_mutable_array_refs,
                        m_vector_array_or_single_refs,
                        m_vector_arrays,
                        *m_signature);
      return m_params;
    }
  };

  virtual void call(ArrayRef<uint> mask_indices, Params &params) const = 0;

  void set_signature(SignatureBuilder &signature_builder)
  {
    m_signature = signature_builder.build();
  }

  const Signature &signature() const
  {
    return m_signature;
  }

 private:
  Signature m_signature;
};

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_H__ */
