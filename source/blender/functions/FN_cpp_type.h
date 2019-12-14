#ifndef __FN_CPP_TYPE_H__
#define __FN_CPP_TYPE_H__

#include "BLI_string_ref.h"
#include "BLI_utility_mixins.h"
#include "BLI_vector.h"

namespace FN {

using BLI::ArrayRef;
using BLI::StringRef;
using BLI::StringRefNull;

class CPPType {
 public:
  using ConstructDefaultF = void (*)(void *ptr);
  using ConstructDefaultNF = void (*)(void *ptr, uint n);
  using ConstructDefaultIndicesF = void (*)(void *ptr, ArrayRef<uint> indices);

  using DestructF = void (*)(void *ptr);
  using DestructNF = void (*)(void *ptr, uint n);
  using DestructIndicesF = void (*)(void *ptr, ArrayRef<uint> indices);

  using CopyToInitializedF = void (*)(const void *src, void *dst);
  using CopyToInitializedNF = void (*)(const void *src, void *dst, uint n);
  using CopyToInitializedIndicesF = void (*)(const void *src, void *dst, ArrayRef<uint> indices);

  using CopyToUninitializedF = void (*)(const void *src, void *dst);
  using CopyToUninitializedNF = void (*)(const void *src, void *dst, uint n);
  using CopyToUninitializedIndicesF = void (*)(const void *src, void *dst, ArrayRef<uint> indices);

  using RelocateToInitializedF = void (*)(void *src, void *dst);
  using RelocateToInitializedNF = void (*)(void *src, void *dst, uint n);
  using RelocateToInitializedIndicesF = void (*)(void *src, void *dst, ArrayRef<uint> indices);

  using RelocateToUninitializedF = void (*)(void *src, void *dst);
  using RelocateToUninitializedNF = void (*)(void *src, void *dst, uint n);
  using RelocateToUninitializedIndicesF = void (*)(void *src, void *dst, ArrayRef<uint> indices);

  using FillInitializedF = void (*)(const void *value, void *dst, uint n);
  using FillInitializedIndicesF = void (*)(const void *value, void *dst, ArrayRef<uint> indices);

  using FillUninitializedF = void (*)(const void *value, void *dst, uint n);
  using FillUninitializedIndicesF = void (*)(const void *value, void *dst, ArrayRef<uint> indices);

  CPPType(std::string name,
          uint size,
          uint alignment,
          bool trivially_destructible,
          ConstructDefaultF construct_default,
          ConstructDefaultNF construct_default_n,
          ConstructDefaultIndicesF construct_default_indices,
          DestructF destruct,
          DestructNF destruct_n,
          DestructIndicesF destruct_indices,
          CopyToInitializedF copy_to_initialized,
          CopyToInitializedNF copy_to_initialized_n,
          CopyToInitializedIndicesF copy_to_initialized_indices,
          CopyToUninitializedF copy_to_uninitialized,
          CopyToUninitializedNF copy_to_uninitialized_n,
          CopyToUninitializedIndicesF copy_to_uninitialized_indices,
          RelocateToInitializedF relocate_to_initialized,
          RelocateToInitializedNF relocate_to_initialized_n,
          RelocateToInitializedIndicesF relocate_to_initialized_indices,
          RelocateToUninitializedF relocate_to_uninitialized,
          RelocateToUninitializedNF relocate_to_uninitialized_n,
          RelocateToUninitializedIndicesF relocate_to_uninitialized_indices,
          FillInitializedF fill_initialized,
          FillInitializedIndicesF fill_initialized_indices,
          FillUninitializedF fill_uninitialized,
          FillUninitializedIndicesF fill_uninitialized_indices,
          const CPPType *generalization)
      : m_size(size),
        m_alignment(alignment),
        m_trivially_destructible(trivially_destructible),
        m_construct_default(construct_default),
        m_construct_default_n(construct_default_n),
        m_construct_default_indices(construct_default_indices),
        m_destruct(destruct),
        m_destruct_n(destruct_n),
        m_destruct_indices(destruct_indices),
        m_copy_to_initialized(copy_to_initialized),
        m_copy_to_initialized_n(copy_to_initialized_n),
        m_copy_to_initialized_indices(copy_to_initialized_indices),
        m_copy_to_uninitialized(copy_to_uninitialized),
        m_copy_to_uninitialized_n(copy_to_uninitialized_n),
        m_copy_to_uninitialized_indices(copy_to_uninitialized_indices),
        m_relocate_to_initialized(relocate_to_initialized),
        m_relocate_to_initialized_n(relocate_to_initialized_n),
        m_relocate_to_initialized_indices(relocate_to_initialized_indices),
        m_relocate_to_uninitialized(relocate_to_uninitialized),
        m_relocate_to_uninitialized_n(relocate_to_uninitialized_n),
        m_relocate_to_uninitialized_indices(relocate_to_uninitialized_indices),
        m_fill_initialized(fill_initialized),
        m_fill_initialized_indices(fill_initialized_indices),
        m_fill_uninitialized(fill_uninitialized),
        m_fill_uninitialized_indices(fill_uninitialized_indices),
        m_generalization(generalization),
        m_name(name)
  {
    BLI_assert(is_power_of_2_i(m_alignment));
    BLI_assert(generalization == nullptr ||
               (generalization->size() == size && generalization->alignment() <= alignment));

    m_alignment_mask = m_alignment - 1;
  }

  virtual ~CPPType();

  StringRefNull name() const
  {
    return m_name;
  }

  uint size() const
  {
    return m_size;
  }

  uint alignment() const
  {
    return m_alignment;
  }

  const CPPType *generalization() const
  {
    return m_generalization;
  }

  bool trivially_destructible() const
  {
    return m_trivially_destructible;
  }

  bool pointer_has_valid_alignment(const void *ptr) const
  {
    return (POINTER_AS_UINT(ptr) & m_alignment_mask) == 0;
  }

  void construct_default(void *ptr) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_construct_default(ptr);
  }

  void construct_default_n(void *ptr, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_construct_default_n(ptr, n);
  }

  void construct_default_indices(void *ptr, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_construct_default_indices(ptr, indices);
  }

  void destruct(void *ptr) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_destruct(ptr);
  }

  void destruct_n(void *ptr, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_destruct_n(ptr, n);
  }

  void destruct_indices(void *ptr, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    m_destruct_indices(ptr, indices);
  }

  void copy_to_initialized(const void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_initialized(src, dst);
  }

  void copy_to_initialized_n(const void *src, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_initialized_n(src, dst, n);
  }

  void copy_to_initialized_indices(const void *src, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_initialized_indices(src, dst, indices);
  }

  void copy_to_uninitialized(const void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_uninitialized(src, dst);
  }

  void copy_to_uninitialized_n(const void *src, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_uninitialized_n(src, dst, n);
  }

  void copy_to_uninitialized_indices(const void *src, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_uninitialized_indices(src, dst, indices);
  }

  void relocate_to_initialized(void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_initialized(src, dst);
  }

  void relocate_to_initialized_n(void *src, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_initialized_n(src, dst, n);
  }

  void relocate_to_initialized_indices(void *src, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_initialized_indices(src, dst, indices);
  }

  void relocate_to_uninitialized(void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_uninitialized(src, dst);
  }

  void relocate_to_uninitialized_n(void *src, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_uninitialized_n(src, dst, n);
  }

  void relocate_to_uninitialized_indices(void *src, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_uninitialized_indices(src, dst, indices);
  }

  void fill_initialized(const void *value, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_fill_initialized(value, dst, n);
  }

  void fill_initialized_indices(const void *value, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_fill_initialized_indices(value, dst, indices);
  }

  void fill_uninitialized(const void *value, void *dst, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_fill_uninitialized(value, dst, n);
  }

  void fill_uninitialized_indices(const void *value, void *dst, ArrayRef<uint> indices) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_fill_uninitialized_indices(value, dst, indices);
  }

  bool is_same_or_generalization(const CPPType &other) const
  {
    if (&other == this) {
      return true;
    }
    if (m_generalization == nullptr) {
      return false;
    }
    return m_generalization->is_same_or_generalization(other);
  }

  friend bool operator==(const CPPType &a, const CPPType &b)
  {
    return &a == &b;
  }

  friend bool operator!=(const CPPType &a, const CPPType &b)
  {
    return !(&a == &b);
  }

 private:
  uint m_size;
  uint m_alignment;
  uint m_alignment_mask;
  bool m_trivially_destructible;

  ConstructDefaultF m_construct_default;
  ConstructDefaultNF m_construct_default_n;
  ConstructDefaultIndicesF m_construct_default_indices;

  DestructF m_destruct;
  DestructNF m_destruct_n;
  DestructIndicesF m_destruct_indices;

  CopyToInitializedF m_copy_to_initialized;
  CopyToInitializedNF m_copy_to_initialized_n;
  CopyToInitializedIndicesF m_copy_to_initialized_indices;

  CopyToUninitializedF m_copy_to_uninitialized;
  CopyToUninitializedNF m_copy_to_uninitialized_n;
  CopyToUninitializedIndicesF m_copy_to_uninitialized_indices;

  RelocateToInitializedF m_relocate_to_initialized;
  RelocateToInitializedNF m_relocate_to_initialized_n;
  RelocateToInitializedIndicesF m_relocate_to_initialized_indices;

  RelocateToUninitializedF m_relocate_to_uninitialized;
  RelocateToUninitializedNF m_relocate_to_uninitialized_n;
  RelocateToUninitializedIndicesF m_relocate_to_uninitialized_indices;

  FillInitializedF m_fill_initialized;
  FillInitializedIndicesF m_fill_initialized_indices;

  FillUninitializedF m_fill_uninitialized;
  FillUninitializedIndicesF m_fill_uninitialized_indices;

  const CPPType *m_generalization;
  std::string m_name;
};

template<typename T> const CPPType &CPP_TYPE();

}  // namespace FN

#endif /* __FN_CPP_TYPE_H__ */
