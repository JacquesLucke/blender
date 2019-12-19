#include "FN_cpp_type.h"
#include "cpp_types.h"

#include "BLI_math_cxx.h"

#include "BKE_surface_hook.h"

namespace FN {

void init_cpp_types()
{
}

void free_cpp_types()
{
}

template<typename T> void ConstructDefault_CB(void *ptr)
{
  BLI::construct_default((T *)ptr);
}
template<typename T> void ConstructDefaultN_CB(void *ptr, uint n)
{
  for (uint i = 0; i < n; i++) {
    BLI::construct_default((T *)ptr + i);
  }
}
template<typename T> void ConstructDefaultIndices_CB(void *ptr, IndexMask index_mask)
{
  for (uint i : index_mask.indices()) {
    BLI::construct_default((T *)ptr + i);
  }
}

template<typename T> void Destruct_CB(void *ptr)
{
  BLI::destruct((T *)ptr);
}
template<typename T> void DestructN_CB(void *ptr, uint n)
{
  BLI::destruct_n((T *)ptr, n);
}
template<typename T> void DestructIndices_CB(void *ptr, IndexMask index_mask)
{
  for (uint i : index_mask.indices()) {
    BLI::destruct((T *)ptr + i);
  }
}

template<typename T> void CopyToInitialized_CB(const void *src, void *dst)
{
  *(T *)dst = *(T *)src;
}
template<typename T> void CopyToInitializedN_CB(const void *src, void *dst, uint n)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    dst_[i] = src_[i];
  }
}
template<typename T>
void CopyToInitializedIndices_CB(const void *src, void *dst, IndexMask index_mask)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    dst_[i] = src_[i];
  }
}

template<typename T> void CopyToUninitialized_CB(const void *src, void *dst)
{
  BLI::uninitialized_copy_n((T *)src, 1, (T *)dst);
}
template<typename T> void CopyToUninitializedN_CB(const void *src, void *dst, uint n)
{
  BLI::uninitialized_copy_n((T *)src, n, (T *)dst);
}
template<typename T>
void CopyToUninitializedIndices_CB(const void *src, void *dst, IndexMask index_mask)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    new (dst_ + i) T(src_[i]);
  }
}

template<typename T> void RelocateToInitialized_CB(void *src, void *dst)
{
  BLI::relocate((T *)src, (T *)dst);
}
template<typename T> void RelocateToInitializedN_CB(void *src, void *dst, uint n)
{
  BLI::relocate_n((T *)src, n, (T *)dst);
}
template<typename T>
void RelocateToInitializedIndices_CB(void *src, void *dst, IndexMask index_mask)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  }
}

template<typename T> void RelocateToUninitialized_CB(void *src, void *dst)
{
  BLI::uninitialized_relocate((T *)src, (T *)dst);
}
template<typename T> void RelocateToUninitializedN_CB(void *src, void *dst, uint n)
{
  BLI::uninitialized_relocate_n((T *)src, n, (T *)dst);
}
template<typename T>
void RelocateToUninitializedIndices_CB(void *src, void *dst, IndexMask index_mask)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  }
}

template<typename T> void FillInitialized_CB(const void *value, void *dst, uint n)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    dst_[i] = value_;
  }
}
template<typename T>
void FillInitializedIndices_CB(const void *value, void *dst, IndexMask index_mask)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    dst_[i] = value_;
  }
}

template<typename T> void FillUninitialized_CB(const void *value, void *dst, uint n)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T>
void FillUninitializedIndices_CB(const void *value, void *dst, IndexMask index_mask)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i : index_mask.indices()) {
    new (dst_ + i) T(value_);
  }
}

template<typename T> static std::unique_ptr<const CPPType> create_cpp_type(StringRef name)
{
  const CPPType *type = new CPPType(name,
                                    sizeof(T),
                                    alignof(T),
                                    std::is_trivially_destructible<T>::value,
                                    ConstructDefault_CB<T>,
                                    ConstructDefaultN_CB<T>,
                                    ConstructDefaultIndices_CB<T>,
                                    Destruct_CB<T>,
                                    DestructN_CB<T>,
                                    DestructIndices_CB<T>,
                                    CopyToInitialized_CB<T>,
                                    CopyToInitializedN_CB<T>,
                                    CopyToInitializedIndices_CB<T>,
                                    CopyToUninitialized_CB<T>,
                                    CopyToUninitializedN_CB<T>,
                                    CopyToUninitializedIndices_CB<T>,
                                    RelocateToInitialized_CB<T>,
                                    RelocateToInitializedN_CB<T>,
                                    RelocateToInitializedIndices_CB<T>,
                                    RelocateToUninitialized_CB<T>,
                                    RelocateToUninitializedN_CB<T>,
                                    RelocateToUninitializedIndices_CB<T>,
                                    FillInitialized_CB<T>,
                                    FillInitializedIndices_CB<T>,
                                    FillUninitialized_CB<T>,
                                    FillUninitializedIndices_CB<T>);
  return std::unique_ptr<const CPPType>(type);
}

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME) \
  static std::unique_ptr<const CPPType> CPPTYPE_##IDENTIFIER = create_cpp_type<TYPE_NAME>( \
      STRINGIFY(IDENTIFIER)); \
  template<> const CPPType &CPP_TYPE<TYPE_NAME>() \
  { \
    return *CPPTYPE_##IDENTIFIER; \
  }

MAKE_CPP_TYPE(float, float)
MAKE_CPP_TYPE(uint32_t, uint32_t)
MAKE_CPP_TYPE(uint8_t, uint8_t)
MAKE_CPP_TYPE(bool, bool)
MAKE_CPP_TYPE(ObjectIDHandle, BKE::ObjectIDHandle)
MAKE_CPP_TYPE(ImageIDHandle, BKE::ImageIDHandle)
MAKE_CPP_TYPE(int32, int32_t)
MAKE_CPP_TYPE(rgba_f, BLI::rgba_f)
MAKE_CPP_TYPE(float3, BLI::float3)
MAKE_CPP_TYPE(string, std::string)
MAKE_CPP_TYPE(SurfaceHook, BKE::SurfaceHook)

}  // namespace FN
