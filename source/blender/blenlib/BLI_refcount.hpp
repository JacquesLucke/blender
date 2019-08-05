#pragma once

/* Objects with shared ownership require a reference
 * counter, so that it can be freed when it is not
 * referenced anymore.
 *
 * Classes can subclass RefCounter to be extended
 * with an intrusive reference count (the counter is
 * stored directly in the object). This is necessary,
 * because the object might have to be used by different
 * languages (C, C++, Python).
 *
 * To avoid error-prone manual reference counting,
 * there is an AutoRefCount class that works similar
 * to std::shared, but deals with objects of type
 * RefCounter.
 */

#include <atomic>
#include <utility>
#include "BLI_utildefines.h"

namespace BLI {

class RefCounter {
 private:
  std::atomic<int> m_refcount;

 protected:
  virtual ~RefCounter(){};

  RefCounter() : m_refcount(1)
  {
  }

 public:
  void incref()
  {
    m_refcount.fetch_add(1);
  }

  void decref()
  {
    int new_value = m_refcount.fetch_sub(1) - 1;
    BLI_assert(new_value >= 0);
    if (new_value == 0) {
      delete this;
    }
  }

  int refcount() const
  {
    return m_refcount;
  }
};

template<typename T> class AutoRefCount {
 private:
  T *m_object;

  inline void incref()
  {
    if (m_object) {
      m_object->incref();
    }
  }

  inline void decref()
  {
    if (m_object) {
      m_object->decref();
    }
  }

 public:
  AutoRefCount() : m_object(nullptr)
  {
  }

  AutoRefCount(T *object) : m_object(object)
  {
  }

  template<typename... Args> static AutoRefCount<T> New(Args &&... args)
  {
    T *object = new T(std::forward<Args>(args)...);
    return AutoRefCount<T>(object);
  }

  AutoRefCount(const AutoRefCount &other)
  {
    m_object = other.m_object;
    this->incref();
  }

  AutoRefCount(AutoRefCount &&other)
  {
    m_object = other.m_object;
    other.m_object = nullptr;
  }

  ~AutoRefCount()
  {
    this->decref();
  }

  AutoRefCount &operator=(const AutoRefCount &other)
  {
    if (this == &other) {
      return *this;
    }
    else if (m_object == other.m_object) {
      return *this;
    }
    else {
      this->decref();
      m_object = other.m_object;
      this->incref();
      return *this;
    }
  }

  AutoRefCount &operator=(AutoRefCount &&other)
  {
    if (this == &other) {
      return *this;
    }
    else if (m_object == other.m_object) {
      other.m_object = nullptr;
      return *this;
    }
    else {
      this->decref();
      m_object = other.m_object;
      other.m_object = nullptr;
      return *this;
    }
  }

  T *ptr() const
  {
    return m_object;
  }

  T &ref() const
  {
    BLI_assert(m_object);
    return *m_object;
  }

  T *extract_ptr()
  {
    T *value = m_object;
    m_object = nullptr;
    return value;
  }

  T *operator->() const
  {
    return this->ptr();
  }

  friend bool operator==(const AutoRefCount &a, const AutoRefCount &b)
  {
    BLI_assert(a.ptr());
    BLI_assert(b.ptr());
    return *a.ptr() == *b.ptr();
  }

  friend bool operator!=(const AutoRefCount &a, const AutoRefCount &b)
  {
    return !(a == b);
  }
};

} /* namespace BLI */

namespace std {
template<typename T> struct hash<BLI::AutoRefCount<T>> {
  typedef BLI::AutoRefCount<T> argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    return std::hash<T>{}(*v.ptr());
  }
};
}  // namespace std
