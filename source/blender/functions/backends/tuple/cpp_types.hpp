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
 public:
  static const uint TYPE_EXTENSION_ID = 0;

  virtual ~CPPTypeInfo()
  {
  }

  /**
   * Get the size of the type in bytes.
   */
  virtual uint size() const = 0;

  /**
   * Get the alignment requirements for this type.
   */
  virtual uint alignment() const = 0;

  /**
   * Construct a default version of that type at the given pointer.
   */
  virtual void construct_default(void *ptr) const = 0;

  /**
   * Destruct the value at the given pointer.
   */
  virtual void destruct_type(void *ptr) const = 0;

  /**
   * Copy the value from src to dst. The destination buffer already contains another instance of
   * the same type which should be overriden.
   */
  virtual void copy_to_initialized(void *src, void *dst) const = 0;

  /**
   * Copy the value from src to dst. The destination buffer contains uninitialized memory.
   */
  virtual void copy_to_uninitialized(void *src, void *dst) const = 0;

  /**
   * Copy the value from src to dst and destroy the original value in src. The destination buffer
   * already contains another instance of the same type which should be overriden.
   */
  virtual void relocate_to_initialized(void *src, void *dst) const = 0;

  /**
   * Copy the value from src to dst and destroy the original value in src. The destination buffer
   * contains uninitialized memory.
   */
  virtual void relocate_to_uninitialized(void *src, void *dst) const = 0;

  /**
   * Return true when the type can be destructed without doing anything. Otherwise false.
   * This is just a hint to improve performance in some cases.
   */
  virtual bool trivially_destructible() const = 0;
};

template<typename T> class CPPTypeInfoForType : public CPPTypeInfo {
 public:
  uint size() const override
  {
    return sizeof(T);
  }

  uint alignment() const override
  {
    return std::alignment_of<T>::value;
  }

  void construct_default(void *ptr) const override
  {
    new (ptr) T();
  }

  void destruct_type(void *ptr) const override
  {
    T *ptr_ = (T *)ptr;
    ptr_->~T();
  }

  void copy_to_initialized(void *src, void *dst) const override
  {
    T *dst_ = (T *)dst;
    T *src_ = (T *)src;
    std::copy(src_, src_ + 1, dst_);
  }

  void copy_to_uninitialized(void *src, void *dst) const override
  {
    T *dst_ = (T *)dst;
    T *src_ = (T *)src;
    std::uninitialized_copy(src_, src_ + 1, dst_);
  }

  void relocate_to_initialized(void *src, void *dst) const override
  {
    T *dst_ = (T *)dst;
    T *src_ = (T *)src;
    *dst_ = std::move(*src_);
    src_->~T();
  }

  void relocate_to_uninitialized(void *src, void *dst) const override
  {
    T *dst_ = (T *)dst;
    T *src_ = (T *)src;
    std::uninitialized_copy(
        std::make_move_iterator(src_), std::make_move_iterator(src_ + 1), dst_);
    src_->~T();
  }

  bool trivially_destructible() const override
  {
    return std::is_trivially_destructible<T>::value;
  }
};

} /* namespace FN */
