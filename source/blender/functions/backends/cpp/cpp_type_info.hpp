#pragma once

/**
 * The CPPTypeInfo class is a type extension for the C++ backend. It contains run-time type
 * information for an arbitrary C++ type.
 *
 * Usually, the class does not have to be subclassed manually, because there is a template that
 * implements all methods for any C++ type automatically.
 */

#include "FN_core.hpp"

#ifdef DEBUG
#  include <typeinfo>
#endif

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

#ifdef DEBUG
  virtual bool has_type_info(const std::type_info &type) const = 0;
#endif
};

/**
 * This implements all necessary functions based on the default-constructor, copy-constructor,
 * move-constructor and destructor of the given class. To get appropriate
 * construction/copy/relocate/free semantics for pointer types, use one of the wrappers below.
 */
template<typename T> class CPPTypeInfoForType : public CPPTypeInfo {
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
    BLI::destruct((T *)ptr);
  }

  void destruct_n(void *ptr, uint n) const override
  {
    BLI::destruct_n((T *)ptr, n);
  }

  void copy_to_initialized(void *src, void *dst) const override
  {
    T *src_ = (T *)src;
    T *dst_ = (T *)dst;
    *dst_ = *src_;
  }

  void copy_to_initialized_n(void *src, void *dst, uint n) const override
  {
    BLI::copy_n((T *)src, n, (T *)dst);
  }

  void copy_to_uninitialized(void *src, void *dst) const override
  {
    T &src_ = *(T *)src;
    new (dst) T(src_);
  }

  void copy_to_uninitialized_n(void *src, void *dst, uint n) const override
  {
    BLI::uninitialized_copy_n((T *)src, n, (T *)dst);
  }

  void relocate_to_initialized(void *src, void *dst) const override
  {
    BLI::relocate((T *)src, (T *)dst);
  }

  void relocate_to_initialized_n(void *src, void *dst, uint n) const override
  {
    BLI::relocate_n((T *)src, n, (T *)dst);
  }

  void relocate_to_uninitialized(void *src, void *dst) const override
  {
    BLI::uninitialized_relocate((T *)src, (T *)dst);
  }

  void relocate_to_uninitialized_n(void *src, void *dst, uint n) const override
  {
    BLI::uninitialized_relocate_n((T *)src, n, (T *)dst);
  }

#ifdef DEBUG
  bool has_type_info(const std::type_info &type) const override
  {
    return type == typeid(T);
  }
#endif
};

/**
 * Use this when the pointer is owned by someone else. The main purpose of this class is to have a
 * pointer that is nullptr when default-initialized.
 */
template<typename T> class ReferencedPointerWrapper {
 private:
  T *m_ptr;

 public:
  ReferencedPointerWrapper() : m_ptr(nullptr)
  {
  }

  ReferencedPointerWrapper(T *ptr) : m_ptr(ptr)
  {
  }

  ReferencedPointerWrapper(const ReferencedPointerWrapper &other) = default;
  ReferencedPointerWrapper(ReferencedPointerWrapper &&other) = default;
  ReferencedPointerWrapper &operator=(const ReferencedPointerWrapper &other) = default;
  ReferencedPointerWrapper &operator=(ReferencedPointerWrapper &&other) = default;

  T *operator->()
  {
    return m_ptr;
  }

  T *ptr()
  {
    return m_ptr;
  }
};

template<typename T> class UniquePointerWrapper {
 private:
  T *m_ptr;

 public:
  UniquePointerWrapper() : m_ptr(nullptr)
  {
  }

  UniquePointerWrapper(T *ptr) : m_ptr(ptr)
  {
  }

  UniquePointerWrapper(const UniquePointerWrapper &other)
  {
    m_ptr = new T(other.ref());
  }

  UniquePointerWrapper(UniquePointerWrapper &&other)
  {
    m_ptr = other.m_ptr;
    other.m_ptr = nullptr;
  }

  ~UniquePointerWrapper()
  {
    if (m_ptr != nullptr) {
      delete m_ptr;
    }
  }

  UniquePointerWrapper &operator=(const UniquePointerWrapper &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~UniquePointerWrapper();
    new (this) UniquePointerWrapper(other);
    return *this;
  }

  UniquePointerWrapper &operator=(UniquePointerWrapper &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~UniquePointerWrapper();
    new (this) UniquePointerWrapper(std::move(other));
    return *this;
  }

  const T *ptr() const
  {
    return m_ptr;
  }

  T *ptr()
  {
    return m_ptr;
  }

  const T &ref() const
  {
    return *m_ptr;
  }

  T &ref()
  {
    return *m_ptr;
  }

  T *operator->()
  {
    return m_ptr;
  }
};

/**
 * The class has to have a clone() method.
 */
template<typename T> class UniqueVirtualPointerWrapper {
 private:
  T *m_ptr;

 public:
  UniqueVirtualPointerWrapper() : m_ptr(nullptr)
  {
  }

  UniqueVirtualPointerWrapper(T *ptr) : m_ptr(ptr)
  {
  }

  UniqueVirtualPointerWrapper(const UniqueVirtualPointerWrapper &other)
  {
    if (other.m_ptr == nullptr) {
      m_ptr = nullptr;
    }
    else {
      m_ptr = other.m_ptr->clone();
    }
  }

  UniqueVirtualPointerWrapper(UniqueVirtualPointerWrapper &&other)
  {
    m_ptr = other.m_ptr;
    other.m_ptr = nullptr;
  }

  ~UniqueVirtualPointerWrapper()
  {
    if (m_ptr != nullptr) {
      delete m_ptr;
    }
  }

  UniqueVirtualPointerWrapper &operator=(const UniqueVirtualPointerWrapper &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~UniqueVirtualPointerWrapper();
    new (this) UniqueVirtualPointerWrapper(other);
    return *this;
  }

  UniqueVirtualPointerWrapper &operator=(UniqueVirtualPointerWrapper &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~UniqueVirtualPointerWrapper();
    new (this) UniqueVirtualPointerWrapper(std::move(other));
    return *this;
  }

  T *ptr()
  {
    return m_ptr;
  }

  T *operator->()
  {
    return m_ptr;
  }

  std::unique_ptr<T> get_unique_copy() const
  {
    T *value = m_ptr->clone();
    return std::unique_ptr<T>(value);
  }
};

} /* namespace FN */
