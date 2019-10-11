#ifndef __BKE_MULTI_FUNCTION_H__
#define __BKE_MULTI_FUNCTION_H__

#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"

namespace BKE {

class MultiFunction {
 public:
  class Signature {
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

  class Params {
   public:
    template<typename T> ArrayOrSingleRef<T> readonly_single_input(uint index, StringRef name);
    GenericArrayOrSingleRef readonly_single_input(uint index, StringRef name);

    template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name);
    GenericMutableArrayRef single_output(uint index, StringRef name);

    template<typename T>
    const GenericVectorArrayOrSingleRef::TypedRef<T> readonly_vector_input(uint index,
                                                                           StringRef name);
    GenericVectorArrayOrSingleRef readonly_vector_input(uint index, StringRef name);

    template<typename T>
    GenericVectorArray::MutableTypedRef<T> vector_output(uint index, StringRef name);
    GenericVectorArray &vector_output(uint index, StringRef name);

    GenericVectorArray &mutable_vector(uint index, StringRef name);
  };

  virtual void signature(Signature &signature) const = 0;
  virtual void call(ArrayRef<uint> mask_indices, Params &params) const = 0;
};

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTION_H__ */
