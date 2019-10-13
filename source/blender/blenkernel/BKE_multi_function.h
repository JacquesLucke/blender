#ifndef __BKE_MULTI_FUNCTION_H__
#define __BKE_MULTI_FUNCTION_H__

#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"

namespace BKE {

class MultiFunction {
 public:
  class SignatureBuilder {
   public:
    template<typename T> void readonly_single_input(StringRef name);
    void readonly_single_input(StringRef name, CPPType &type);

    template<typename T> void single_output(StringRef name);
    void single_output(StringRef name, CPPType &base_type);

    template<typename T> void readonly_vector_input(StringRef name);
    void readonly_vector_input(StringRef name, CPPType &base_type);

    template<typename T> void vector_output(StringRef name);
    void vector_output(StringRef name, CPPType &base_type);

    void mutable_vector(StringRef name, CPPType &base_type);
  };

  class Signature {
   public:
    uint get_corrected_index(uint index);

    template<typename T> bool is_readonly_single_input(uint index, StringRef name) const;
    bool is_readonly_single_input(uint index, StringRef name) const;

    template<typename T> bool is_single_output(uint index, StringRef name) const;
    bool is_single_output(uint index, StringRef name) const;

    template<typename T> bool is_readonly_vector_input(uint index, StringRef name) const;
    bool is_readonly_vector_input(uint index, StringRef name) const;

    template<typename T> bool is_vector_output(uint index, StringRef name) const;
    bool is_vector_output(uint index, StringRef name);

    bool is_mutable_vector(uint index, StringRef name) const;
  };

  class Params {
   public:
    template<typename T> ArrayOrSingleRef<T> readonly_single_input(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_readonly_single_input<T>(index, name));
      return this->readonly_single_input(index, name).as_typed_ref<T>();
    }
    GenericArrayOrSingleRef readonly_single_input(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_readonly_single_input(index, name));
      uint corrected_index = m_signature.get_corrected_index(index);
      return m_array_or_single_refs[corrected_index];
    }

    template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_single_output<T>(index, name));
      return this->single_output(index, name).get_ref<T>();
    }
    GenericMutableArrayRef single_output(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_single_output(index, name));
      uint corrected_index = m_signature.get_corrected_index(index);
      return m_mutable_array_refs[corrected_index];
    }

    template<typename T>
    const GenericVectorArrayOrSingleRef::TypedRef<T> readonly_vector_input(uint index,
                                                                           StringRef name)
    {
      BLI_assert(m_signature.is_readonly_vector_input<T>(index, name));
      return this->readonly_vector_input(index, name).as_typed_ref<T>();
    }
    GenericVectorArrayOrSingleRef readonly_vector_input(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_readonly_vector_input(index, name));
      uint corrected_index = m_signature.get_corrected_index(index);
      return m_vector_array_or_single_refs[corrected_index];
    }

    template<typename T>
    GenericVectorArray::MutableTypedRef<T> vector_output(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_vector_output<T>(index, name));
      return this->vector_output(index, name).as_mutable_typed_ref<T>();
    }
    GenericVectorArray &vector_output(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_vector_output(index, name));
      uint corrected_index = m_signature.get_corrected_index(index);
      return *m_vector_arrays[corrected_index];
    }

    GenericVectorArray &mutable_vector(uint index, StringRef name)
    {
      BLI_assert(m_signature.is_mutable_vector(index, name));
      uint corrected_index = m_signature.get_corrected_index(index);
      return *m_vector_arrays[corrected_index];
    }

   private:
    ArrayRef<GenericArrayOrSingleRef> m_array_or_single_refs;
    ArrayRef<GenericMutableArrayRef> m_mutable_array_refs;
    ArrayRef<GenericVectorArrayOrSingleRef> m_vector_array_or_single_refs;
    ArrayRef<GenericVectorArray *> m_vector_arrays;
    Signature &m_signature;
  };

  virtual void signature(SignatureBuilder &signature) const = 0;
  virtual void call(ArrayRef<uint> mask_indices, Params &params) const = 0;
};

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_H__ */
