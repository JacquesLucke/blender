#pragma once

#include <string>
#include <mutex>
#include "BLI_composition.hpp"
#include "BLI_shared.hpp"
#include "BLI_string_ref.hpp"

namespace FN {

using namespace BLI;

class Type;

class TypeExtension {
 private:
  Type *m_owner = nullptr;
  friend Type;

  void set_owner(Type *owner);

 public:
  virtual ~TypeExtension();

  Type *owner() const;
};

class Type final : public RefCountedBase {
 public:
  Type() = delete;
  Type(StringRef name);

  const StringRefNull name() const;

  template<typename T> bool has_extension() const;
  template<typename T> T *extension() const;
  template<typename T, typename... Args> bool add_extension(Args &&... args);

 private:
  std::string m_name;
  Composition m_extensions;
  mutable std::mutex m_extension_mutex;
};

using SharedType = AutoRefCount<Type>;

/* Type inline functions
 ****************************************/

inline Type::Type(StringRef name) : m_name(name.to_std_string())
{
}

inline const StringRefNull Type::name() const
{
  return m_name;
}

template<typename T> inline bool Type::has_extension() const
{
  std::lock_guard<std::mutex> lock(m_extension_mutex);
  static_assert(std::is_base_of<TypeExtension, T>::value, "");
  return m_extensions.has<T>();
}

template<typename T> inline T *Type::extension() const
{
  /* TODO: Check if we really need a lock here.
   *   Since extensions can't be removed, it might be
   *   to access existing extensions without a lock. */
  std::lock_guard<std::mutex> lock(m_extension_mutex);
  static_assert(std::is_base_of<TypeExtension, T>::value, "");
  return m_extensions.get<T>();
}

template<typename T, typename... Args> inline bool Type::add_extension(Args &&... args)
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

inline bool operator==(const Type &a, const Type &b)
{
  return &a == &b;
}

/* Type Extension inline functions
 ****************************************/

inline void TypeExtension::set_owner(Type *owner)
{
  m_owner = owner;
}

inline Type *TypeExtension::owner() const
{
  return m_owner;
}

} /* namespace FN */

/* Make Type hashable using std::hash.
 ****************************************/

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
