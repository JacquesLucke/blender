/**
 * The type system is a fundamental part of the functions system. It is essentially a runtime RTTI
 * (run-time type information) system that can support multiple execution backends (e.g. C++, LLVM,
 * GLSL).
 *
 * The Type class is a container for a specific type. A type is identified by its pointer at
 * run-time. Every type also has a name, but that should only be used for e.g. debugging and not as
 * identifier.
 *
 * A Type instance can contain an arbitrary amount of type extensions. By having multiple
 * extensions for the same type, it can be used by multiple execution backends.
 *
 * Type extensions are identified by their C++ type. So, every type can have each extension type at
 * most once.
 *
 * A type owns its extensions. They can be dynamically added, but not removed. The extensions are
 * freed whenever the type is freed.
 */

#pragma once

#include <string>
#include <mutex>
#include "BLI_refcount.hpp"
#include "BLI_string_ref.hpp"
#include "MEM_guardedalloc.h"
#include "BLI_utility_mixins.hpp"

namespace FN {

using namespace BLI;

class Type;

class TypeExtension : BLI::NonCopyable, BLI::NonMovable {
 private:
  Type *m_owner = nullptr;
  friend Type;

  void set_owner(Type *owner);

 public:
  TypeExtension() = default;

  virtual ~TypeExtension();

  Type *owner() const;

  static const uint EXTENSION_TYPE_AMOUNT = 2;
};

class Type final {
 public:
  Type() = delete;
  Type(StringRef name);
  ~Type();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("FN:Types")
#endif

  /**
   * Get the name of the type.
   */
  const StringRefNull name() const;

  /**
   * Return true, when the type has an extension of type T. Otherwise false.
   */
  template<typename T> bool has_extension() const;

  /**
   * Return the extension of type T.
   * Asserts when the extension does not exist.
   */
  template<typename T> T &extension() const;

  /**
   * Add a new extension of type T to the type. It will be constructed using the args passed to
   * this function. When this function is called multiple types with the same T, only the first
   * call will change the type.
   */
  template<typename T, typename... Args> bool add_extension(Args &&... args);

 private:
  std::string m_name;
  std::mutex m_add_extension_mutex;
  TypeExtension *m_extensions[TypeExtension::EXTENSION_TYPE_AMOUNT] = {0};
};

/* Type inline functions
 ****************************************/

inline Type::Type(StringRef name) : m_name(name)
{
}

inline const StringRefNull Type::name() const
{
  return m_name;
}

#define STATIC_ASSERT_EXTENSION_TYPE(T) \
  BLI_STATIC_ASSERT((std::is_base_of<TypeExtension, T>::value), ""); \
  BLI_STATIC_ASSERT(T::TYPE_EXTENSION_ID < TypeExtension::EXTENSION_TYPE_AMOUNT, "")

template<typename T> inline bool Type::has_extension() const
{
  STATIC_ASSERT_EXTENSION_TYPE(T);
  return m_extensions[T::TYPE_EXTENSION_ID] != nullptr;
}

template<typename T> inline T &Type::extension() const
{
  STATIC_ASSERT_EXTENSION_TYPE(T);
  BLI_assert(this->has_extension<T>());
  return *(T *)m_extensions[T::TYPE_EXTENSION_ID];
}

template<typename T, typename... Args> inline bool Type::add_extension(Args &&... args)
{
  STATIC_ASSERT_EXTENSION_TYPE(T);

  std::lock_guard<std::mutex> lock(m_add_extension_mutex);
  if (m_extensions[T::TYPE_EXTENSION_ID] == nullptr) {
    T *new_extension = new T(std::forward<Args>(args)...);
    new_extension->set_owner(this);
    m_extensions[T::TYPE_EXTENSION_ID] = new_extension;
    return true;
  }
  else {
    return false;
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

namespace BLI {
template<> struct DefaultHash<FN::Type> {
  uint32_t operator()(const FN::Type &value) const noexcept
  {
    void *ptr = (void *)&value;
    return DefaultHash<void *>{}(ptr);
  }
};
}  // namespace BLI
