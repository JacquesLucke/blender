#ifndef __BKE_MULTI_FUNCTION_H__
#define __BKE_MULTI_FUNCTION_H__

#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"
#include "BKE_tuple.h"

#include "BLI_vector.h"

namespace BKE {

using BLI::Vector;

struct ParamType {
 public:
  enum Category {
    None,
    SingleInput,
    SingleOutput,
    VectorInput,
    VectorOutput,
    MutableVector,
  };

  ParamType(Category category, const CPPType *base_type = nullptr)
      : m_category(category), m_base_type(base_type)
  {
  }

  Category category() const
  {
    return m_category;
  }

  const CPPType &type() const
  {
    BLI_assert(ELEM(m_category, Category::SingleInput, Category::SingleOutput));
    return *m_base_type;
  }

  const CPPType &base_type() const
  {
    BLI_assert(
        ELEM(m_category, Category::VectorInput, Category::VectorOutput, Category::MutableVector));
    return *m_base_type;
  }

 private:
  Category m_category = Category::None;
  const CPPType *m_base_type = nullptr;
};

struct DataType {
 public:
  enum Category {
    None,
    Single,
    Vector,
  };

  DataType(Category category, const CPPType &type) : m_category(category), m_base_type(&type)
  {
  }

  Category category() const
  {
    return m_category;
  }

  const CPPType &type() const
  {
    BLI_assert(m_category == Category::Single);
    return *m_base_type;
  }

  const CPPType &base_type() const
  {
    BLI_assert(m_category == Category::Vector);
    return *m_base_type;
  }

 private:
  Category m_category = Category::None;
  const CPPType *m_base_type = nullptr;
};

class MultiFunction {
 public:
  class Signature {
   private:
    Vector<std::string> m_param_names;
    Vector<ParamType> m_param_types;
    Vector<uint> m_corrected_indices;

   public:
    Signature() = default;

    Signature(Vector<std::string> param_names, Vector<ParamType> param_types)
        : m_param_names(std::move(param_names)), m_param_types(std::move(param_types))
    {
      uint array_or_single_refs = 0;
      uint mutable_array_refs = 0;
      uint vector_array_or_single_refs = 0;
      uint vector_arrays = 0;
      for (ParamType param_type : m_param_types) {
        uint corrected_index = 0;
        switch (param_type.category()) {
          case ParamType::None:
            BLI_assert(false);
            break;
          case ParamType::SingleInput:
            corrected_index = array_or_single_refs++;
            break;
          case ParamType::SingleOutput:
            corrected_index = mutable_array_refs++;
            break;
          case ParamType::VectorInput:
            corrected_index = vector_array_or_single_refs++;
            break;
          case ParamType::VectorOutput:
          case ParamType::MutableVector:
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
      return this->is_valid_param<T>(index, name, ParamType::SingleInput);
    }
    bool is_readonly_single_input(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamType::SingleInput);
    }

    template<typename T> bool is_single_output(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamType::SingleOutput);
    }
    bool is_single_output(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamType::SingleOutput);
    }

    template<typename T> bool is_readonly_vector_input(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamType::VectorInput);
    }
    bool is_readonly_vector_input(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamType::VectorInput);
    }

    template<typename T> bool is_vector_output(uint index, StringRef name) const
    {
      return this->is_valid_param<T>(index, name, ParamType::VectorOutput);
    }
    bool is_vector_output(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamType::VectorOutput);
    }

    bool is_mutable_vector(uint index, StringRef name) const
    {
      return this->is_valid_param(index, name, ParamType::MutableVector);
    }

   private:
    template<typename T>
    bool is_valid_param(uint index, StringRef name, ParamType::Category category) const
    {
      if (!this->is_valid_param(index, name, category)) {
        return false;
      }
      else if (ELEM(category, ParamType::SingleInput, ParamType::SingleOutput)) {
        return GET_TYPE<T>().is_same_or_generalization(m_param_types[index].type());
      }
      else if (ELEM(category,
                    ParamType::VectorInput,
                    ParamType::VectorOutput,
                    ParamType::MutableVector)) {
        return GET_TYPE<T>().is_same_or_generalization(m_param_types[index].base_type());
      }
      else {
        return false;
      }
    }

    bool is_valid_param(uint index, StringRef name, ParamType::Category category) const
    {
      return m_param_names[index] == name && m_param_types[index].category() == category;
    }
  };

  class SignatureBuilder {
   private:
    Vector<std::string> m_param_names;
    Vector<ParamType> m_param_types;
    Vector<CPPType *> m_param_base_types;

   public:
    template<typename T> void readonly_single_input(StringRef name)
    {
      this->readonly_single_input(name, GET_TYPE<T>());
    }
    void readonly_single_input(StringRef name, CPPType &type)
    {
      m_param_names.append(name);
      m_param_types.append(ParamType(ParamType::SingleInput, &type));
    }

    template<typename T> void single_output(StringRef name)
    {
      this->single_output(name, GET_TYPE<T>());
    }
    void single_output(StringRef name, CPPType &type)
    {
      m_param_names.append(name);
      m_param_types.append(ParamType(ParamType::SingleOutput, &type));
    }

    template<typename T> void readonly_vector_input(StringRef name)
    {
      this->readonly_vector_input(name, GET_TYPE<T>());
    }
    void readonly_vector_input(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_types.append(ParamType(ParamType::VectorInput, &base_type));
    }

    template<typename T> void vector_output(StringRef name)
    {
      this->vector_output(name, GET_TYPE<T>());
    }
    void vector_output(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_types.append(ParamType(ParamType::VectorOutput, &base_type));
    }

    void mutable_vector(StringRef name, CPPType &base_type)
    {
      m_param_names.append(name);
      m_param_types.append(ParamType(ParamType::MutableVector, &base_type));
    }

    Signature build()
    {
      return Signature(std::move(m_param_names), std::move(m_param_types));
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

    void add_readonly_array_ref(GenericMutableArrayRef array)
    {
      BLI_assert(array.size() >= m_min_array_size);
      m_array_or_single_refs.append(
          GenericArrayOrSingleRef::FromArray(array.type(), array.buffer(), array.size()));
    }

    void add_readonly_single_ref(TupleRef tuple, uint index)
    {
      m_array_or_single_refs.append(GenericArrayOrSingleRef::FromSingle(
          tuple.info().type_at_index(index), tuple.element_ptr(index), m_min_array_size));
    }

    template<typename T> void add_mutable_array_ref(ArrayRef<T> array)
    {
      BLI_assert(array.size() >= m_min_array_size);
      m_mutable_array_refs.append(GenericMutableArrayRef(array));
    }

    void add_mutable_array_ref(GenericMutableArrayRef array)
    {
      BLI_assert(array.size() >= m_min_array_size);
      m_mutable_array_refs.append(array);
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
