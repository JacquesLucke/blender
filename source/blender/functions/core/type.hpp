#pragma once

#include <string>
#include <mutex>
#include "BLI_composition.hpp"
#include "BLI_shared.hpp"

namespace FN {

using namespace BLI;

class Type;

class TypeExtension {
 private:
  Type *m_owner = nullptr;
  friend Type;

  void set_owner(Type *owner)
  {
    m_owner = owner;
  }

 public:
  virtual ~TypeExtension()
  {
  }

  Type *owner() const
  {
    return m_owner;
  }
};

class Type final : public RefCountedBase {
 public:
  Type() = delete;
  Type(const std::string &name) : m_name(name)
  {
  }

  const std::string &name() const
  {
    return m_name;
  }

  template<typename T> bool has_extension() const
  {
    std::lock_guard<std::mutex> lock(m_extension_mutex);
    static_assert(std::is_base_of<TypeExtension, T>::value, "");
    return m_extensions.has<T>();
  }

  template<typename T> T *extension() const
  {
    /* TODO: Check if we really need a lock here.
     *   Since extensions can't be removed, it might be
     *   to access existing extensions without a lock. */
    std::lock_guard<std::mutex> lock(m_extension_mutex);
    static_assert(std::is_base_of<TypeExtension, T>::value, "");
    return m_extensions.get<T>();
  }

  template<typename T, typename... Args> bool add_extension(Args &&... args)
  {
    std::lock_guard<std::mutex> lock(m_extension_mutex);
    static_assert(std::is_base_of<TypeExtension, T>::value, "");

    if (m_extensions.has<T>()) {
      return false;
    }
    else {
      T *new_extension = new T(std::forward<Args>(args)...);
      new_extension->set_owner(this);
      m_extensions.add(new_extension);
      return true;
    }
  }

  friend bool operator==(const Type &a, const Type &b)
  {
    return &a == &b;
  }

 private:
  std::string m_name;
  Composition m_extensions;
  mutable std::mutex m_extension_mutex;
};

using SharedType = AutoRefCount<Type>;
using TypeVector = SmallVector<SharedType>;

} /* namespace FN */

namespace std {
template<> struct hash<FN::Type> {
  typedef FN::Type argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    return std::hash<void *>{}((void *)&v);
  }
};
}  // namespace std
