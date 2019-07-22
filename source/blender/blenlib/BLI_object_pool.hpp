#pragma once

/* This structure allows reusing the same object.
 *
 * Aquire:
 *   Get an object that might have been used before.
 *   If no unused object exists currently, a new one
 *   will be allocated and constructed.
 *
 * Release:
 *   Give back the object instance, so that someone
 *   else can use it later. The destructor is not
 *   necessarily called.
 */

#include <mutex>

#include "BLI_stack.hpp"
#include "BLI_set.hpp"

namespace BLI {

template<typename T> class ThreadSafeObjectPool {
 private:
  std::mutex m_mutex;
  Stack<T *> m_free_objects;

#ifdef DEBUG
  Set<T *> m_all_objects;
#else
  SmallVector<T *> m_all_objects;
#endif

 public:
  ThreadSafeObjectPool() = default;
  ThreadSafeObjectPool(ThreadSafeObjectPool &other) = delete;

  ~ThreadSafeObjectPool()
  {
    BLI_assert(m_free_objects.size() == m_all_objects.size());
    for (T *object : m_all_objects) {
      delete object;
    }
  }

  T *aquire()
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_free_objects.empty()) {
      T *new_object = new T();
      this->remember(new_object);
      return new_object;
    }
    else {
      return m_free_objects.pop();
    }
  }

  void release(T *object)
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    BLI_assert(m_all_objects.contains(object));
    m_free_objects.push(object);
  }

 private:
  void remember(T *object)
  {
#ifdef DEBUG
    m_all_objects.add(object);
#else
    m_all_objects.append(object);
#endif
  }
};

}  // namespace BLI
