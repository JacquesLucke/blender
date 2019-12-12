#ifndef __FN_CPP_TYPE_H__
#define __FN_CPP_TYPE_H__

#include "BLI_string_ref.h"
#include "BLI_utility_mixins.h"
#include "BLI_vector.h"

namespace FN {

using BLI::StringRef;
using BLI::StringRefNull;

class CPPType {
 public:
  using ConstructDefaultF = void (*)(void *ptr);
  using ConstructDefaultNF = void (*)(void *ptr, uint n);
  using DestructF = void (*)(void *ptr);
  using DestructNF = void (*)(void *ptr, uint n);
  using CopyToInitializedF = void (*)(const void *src, void *dst);
  using CopyToUninitializedF = void (*)(const void *src, void *dst);
  using RelocateToInitializedF = void (*)(void *src, void *dst);
  using RelocateToUninitializedF = void (*)(void *src, void *dst);
  using RelocateToUninitializedNF = void (*)(void *src, void *dst, uint n);

  CPPType(std::string name,
          uint size,
          uint alignment,
          bool trivially_destructible,
          ConstructDefaultF construct_default,
          ConstructDefaultNF construct_default_n,
          DestructF destruct,
          DestructNF destruct_n,
          CopyToInitializedF copy_to_initialized,
          CopyToUninitializedF copy_to_uninitialized,
          RelocateToInitializedF relocate_to_initialized,
          RelocateToUninitializedF relocate_to_uninitialized,
          RelocateToUninitializedNF relocate_to_uninitialized_n,
          const CPPType *generalization)
      : m_size(size),
        m_alignment(alignment),
        m_trivially_destructible(trivially_destructible),
        m_construct_default(construct_default),
        m_construct_default_n(construct_default_n),
        m_destruct(destruct),
        m_destruct_n(destruct_n),
        m_copy_to_initialized(copy_to_initialized),
        m_copy_to_uninitialized(copy_to_uninitialized),
        m_relocate_to_initialized(relocate_to_initialized),
        m_relocate_to_uninitialized(relocate_to_uninitialized),
        m_relocate_to_uninitialized_n(relocate_to_uninitialized_n),
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

  void copy_to_initialized(const void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_initialized(src, dst);
  }

  void copy_to_uninitialized(const void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_copy_to_uninitialized(src, dst);
  }

  void relocate_to_initialized(void *src, void *dst) const
  {
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    m_relocate_to_initialized(src, dst);
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
  DestructF m_destruct;
  DestructNF m_destruct_n;
  CopyToInitializedF m_copy_to_initialized;
  CopyToUninitializedF m_copy_to_uninitialized;
  RelocateToInitializedF m_relocate_to_initialized;
  RelocateToUninitializedF m_relocate_to_uninitialized;
  RelocateToUninitializedNF m_relocate_to_uninitialized_n;
  const CPPType *m_generalization;
  std::string m_name;
};

template<typename T> const CPPType &CPP_TYPE();

}  // namespace FN

#endif /* __FN_CPP_TYPE_H__ */
