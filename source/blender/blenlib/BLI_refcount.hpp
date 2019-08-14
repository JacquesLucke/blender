/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 *
 * Objects with shared ownership require a reference counter, so that they can be freed when they
 * are not referenced anymore.
 *
 * Classes can subclass RefCounter to be extended with an intrusive reference count (the counter is
 * stored directly in the object). This is necessary, because the object might have to be used by
 * different languages (C, C++, Python).
 *
 * To avoid error-prone manual reference counting, there is an AutoRefCount class that works
 * similar to std::shared, but deals with objects of type RefCounter.
 */

#pragma once

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
  /**
   * Increment the reference counter atomically.
   */
  void incref()
  {
    m_refcount.fetch_add(1);
  }

  /**
   * Decrement the reference counter atomically. Deletes the instance if the reference counter
   * becomes zero.
   */
  void decref()
  {
    int new_value = m_refcount.fetch_sub(1) - 1;
    BLI_assert(new_value >= 0);
    if (new_value == 0) {
      delete this;
    }
  }

  /**
   * Get the current reference count.
   */
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

  /**
   * Similar to std::make_shared.
   */
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

  /**
   * Get the pointer that is currently wrapped. This pointer can be null.
   */
  T *ptr() const
  {
    return m_object;
  }

  /**
   * Get a reference to the object that is currently wrapped.
   * Asserts when no object is wrapped.
   */
  T &ref() const
  {
    BLI_assert(m_object);
    return *m_object;
  }

  /**
   * Get the pointer that is currently wrapped and remove it from this automatic reference counter,
   * effectively taking over the ownership. Note that this can return null.
   */
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

  /**
   * They compare equal, when the wrapped objects compare equal.
   * Asserts when one of the two does not wrap an object currently.
   */
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
