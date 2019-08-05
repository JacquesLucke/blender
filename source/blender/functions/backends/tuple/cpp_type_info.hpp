#pragma once

/**
 * The CPPTypeInfo class is a type extension for the C++ backend. It contains run-time type
 * information for an arbitrary C++ type.
 *
 * Usually, the class does not have to be subclassed manually, because there is a template that
 * implements all methods for any C++ type automatically.
 */

#include "FN_core.hpp"

namespace FN {

class CPPTypeInfo : public TypeExtension {
 private:
  uint m_size;
  uint m_alignment;
  bool m_is_trivially_destructible;

 public:
  static const uint TYPE_EXTENSION_ID = 0;

  CPPTypeInfo(uint size, uint alignment, bool is_trivially_destructible)
      : m_size(size),
        m_alignment(alignment),
        m_is_trivially_destructible(is_trivially_destructible)
  {
  }

  virtual ~CPPTypeInfo()
  {
  }

  /**
   * Get the size of the type in bytes.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Get the alignment requirements for this type.
   */
  uint alignment() const
  {
    return m_alignment;
  }

  /**
   * Return true when the type can be destructed without doing anything. Otherwise false.
   * This is just a hint to improve performance in some cases.
   */
  bool trivially_destructible() const
  {
    return m_is_trivially_destructible;
  }

  /**
   * Construct a default version of that type at the given pointer.
   */
  virtual void construct_default(void *ptr) const = 0;
  virtual void construct_default_n(void *ptr, uint n) const = 0;

  /**
   * Destruct the value at the given pointer.
   */
  virtual void destruct(void *ptr) const = 0;
  virtual void destruct_n(void *ptr, uint n) const = 0;

  /**
   * Copy the value from src to dst. The destination buffer already contains another instance of
   * the same type which should be overriden.
   */
  virtual void copy_to_initialized(void *src, void *dst) const = 0;
  virtual void copy_to_initialized_n(void *src, void *dst, uint n) const = 0;

  /**
   * Copy the value from src to dst. The destination buffer contains uninitialized memory.
   */
  virtual void copy_to_uninitialized(void *src, void *dst) const = 0;
  virtual void copy_to_uninitialized_n(void *src, void *dst, uint n) const = 0;

  /**
   * Copy the value from src to dst and destroy the original value in src. The destination buffer
   * already contains another instance of the same type which should be overriden.
   */
  virtual void relocate_to_initialized(void *src, void *dst) const = 0;
  virtual void relocate_to_initialized_n(void *src, void *dst, uint n) const = 0;

  /**
   * Copy the value from src to dst and destroy the original value in src. The destination buffer
   * contains uninitialized memory.
   */
  virtual void relocate_to_uninitialized(void *src, void *dst) const = 0;
  virtual void relocate_to_uninitialized_n(void *src, void *dst, uint n) const = 0;
};

template<typename T> class CPPTypeInfoForType final : public CPPTypeInfo {
 public:
  CPPTypeInfoForType()
      : CPPTypeInfo(
            sizeof(T), std::alignment_of<T>::value, std::is_trivially_destructible<T>::value)
  {
  }

  void construct_default(void *ptr) const override
  {
    new (ptr) T();
  }

  void construct_default_n(void *ptr, uint n) const override
  {
    T *ptr_ = (T *)ptr;
    for (uint i = 0; i < n; i++) {
      new (ptr_ + i) T();
    }
  }

  void destruct(void *ptr) const override
  {
    T *ptr_ = (T *)ptr;
    ptr_->~T();
  }

  void destruct_n(void *ptr, uint n) const override
  {
    T *ptr_ = (T *)ptr;
    for (uint i = 0; i < n; i++) {
      ptr_[i].~T();
    }
  }

  void copy_to_initialized(void *src, void *dst) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::copy_n(src_, 1, dst_);
  }

  void copy_to_initialized_n(void *src, void *dst, uint n) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::copy_n(src_, n, dst_);
  }

  void copy_to_uninitialized(void *src, void *dst) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::uninitialized_copy_n(src_, 1, dst_);
  }

  void copy_to_uninitialized_n(void *src, void *dst, uint n) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::uninitialized_copy_n(src_, n, dst_);
  }

  void relocate_to_initialized(void *src, void *dst) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    *dst_ = std::move(*src_);
    src_->~T();
  }

  void relocate_to_initialized_n(void *src, void *dst, uint n) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::copy_n(std::make_move_iterator(src_), n, dst_);
    for (uint i = 0; i < n; i++) {
      src_[i].~T();
    }
  }

  void relocate_to_uninitialized(void *src, void *dst) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::uninitialized_copy_n(std::make_move_iterator(src_), 1, dst_);
    src_->~T();
  }

  virtual void relocate_to_uninitialized_n(void *src, void *dst, uint n) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    std::uninitialized_copy_n(std::make_move_iterator(src_), n, dst_);
    for (uint i = 0; i < n; i++) {
      src_[i].~T();
    }
  }
};

} /* namespace FN */
