#pragma once

/* This class allows to create a set of objects
 * of different types. Only one instance of a type
 * can exist in the set.
 *
 * In some cases, this approach should be preferred
 * over multiple inheritance.
 */

#include "BLI_map.hpp"

namespace BLI {

class Composition {
 public:
  typedef void (*FreeFunction)(void *value);

 private:
  struct Entry {
    void *value;
    FreeFunction free;

    template<typename T> Entry(T *value) : value((void *)value), free(get_free_function<T>())
    {
    }
  };

 public:
  template<typename T> void add(T *value)
  {
    m_elements.add(this->get_key<T>(), Entry(value));
  }

  template<typename T> inline T *get() const
  {
    uint64_t key = this->get_key<T>();
    if (m_elements.contains(key)) {
      return (T *)m_elements.lookup(key).value;
    }
    else {
      return nullptr;
    }
  }

  template<typename T> inline bool has() const
  {
    return m_elements.contains(this->get_key<T>());
  }

  ~Composition()
  {
    for (const Entry &entry : m_elements.values()) {
      entry.free(entry.value);
    }
  }

 private:
  template<typename T> static uint64_t get_key()
  {
    return (uint64_t)T::identifier_in_composition();
  }

  template<typename T> static FreeFunction get_free_function()
  {
    return T::free_self;
  }

  BLI::Map<uint64_t, Entry> m_elements;
};

} /* namespace BLI */

#define BLI_COMPOSITION_DECLARATION(Type) \
  static const char *identifier_in_composition(); \
  static void free_self(void *value);

#define BLI_COMPOSITION_IMPLEMENTATION(Type) \
  const char *Type::identifier_in_composition() \
  { \
    return STRINGIFY(Type); \
  } \
  void Type::free_self(void *value) \
  { \
    delete (Type *)value; \
  }
