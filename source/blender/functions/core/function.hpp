#pragma once

/**
 * The `Function` class is a fundamental type of the functions system. It generically represents
 * something that has named inputs and outputs of specific types. The function itself does not know
 * about how it is executed, because this differs between different execution backends. It is
 * similar to the declaration of a function in a C program, with two main differences:
 *
 *   - It can have an arbitrary but fixed number of inputs AND outputs.
 *   - It can have multiple implementations. However, every implementation corresponds to a
 *     different execution backend.
 *
 * The ownership semantics of instances of Function are the same as for Type.
 *
 * In the same way types have type extensions, a function has function bodies. These are also
 * identified by their C++ type.
 *
 * The inputs and outputs of a function are immutable after it has been created. New functions
 * should be created using the corresponding builder class.
 */

#include <functional>

#include "BLI_utility_mixins.h"
#include "BLI_resource_collector.h"

#include "type.hpp"

namespace FN {

using BLI::ResourceCollector;
using BLI::Vector;

class Function;

class FunctionBody : BLI::NonCopyable, BLI::NonMovable {
 private:
  Function *m_owner = nullptr;

  void set_owner(Function *fn);

  friend class Function;

 protected:
  virtual void owner_init_post();

 public:
  FunctionBody() = default;

  virtual ~FunctionBody();

  Function &owner() const;

  static const uint BODY_TYPE_AMOUNT = 5;
};

class Function final : BLI::NonCopyable, BLI::NonMovable {
 public:
  /**
   * Construct a new function. Instead of calling this directly, the FunctionBuilder should be
   * used.
   */
  Function(StringRefNull name,
           ArrayRef<StringRefNull> input_names,
           ArrayRef<Type *> input_types,
           ArrayRef<StringRefNull> output_names,
           ArrayRef<Type *> output_types,
           const char *strings);

  ~Function();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("FN:Function")
#endif

  /**
   * Get the name of the function.
   */
  const StringRefNull name() const;

  /**
   * Return true when the function has a body of type T. Otherwise false.
   */
  template<typename T> inline bool has_body() const;

  /**
   * Return a function body of type T.
   * Asserts when the body type does not exist in this function.
   */
  template<typename T> inline T &body() const;

  /**
   * Add another implementation to the function. Every type of implementation can only be added
   * once. Future calls with the same type are ignored.
   * Returns the pointer to the body when it was newly created, nullptr otherwise.
   */
  template<typename T, typename... Args> T *add_body(Args &&... args);

  /**
   * Get the number of inputs.
   */
  uint input_amount() const;

  /**
   * Get the number of outputs.
   */
  uint output_amount() const;

  /**
   * Get the type of the input at the given index.
   */
  Type *input_type(uint index) const;

  /**
   * Get the type of the output at the given index.
   */
  Type *output_type(uint index) const;

  /**
   * Get the name of the input at the given index.
   */
  StringRefNull input_name(uint index) const;

  /**
   * Get the name of the output at the given index.
   */
  StringRefNull output_name(uint index) const;

  /**
   * Utility to get a specific type extension for all inputs.
   * Asserts, when at least one input does not have the extension.
   */
  template<typename T> Vector<T *> input_extensions() const;

  /**
   * Utility to get a specific type extension for all outputs.
   * Asserts when at least one output does not have the extension.
   */
  template<typename T> Vector<T *> output_extensions() const;

  /**
   * Get an array containing all input types.
   */
  ArrayRef<Type *> input_types() const;

  /**
   * Get an array containing all output types.
   */
  ArrayRef<Type *> output_types() const;

  /**
   * Print some debug information for the function.
   */
  void print();

  /**
   * Add a resource that is owned by the function. All resources will be freed in reverse order.
   */
  template<typename T> void add_resource(std::unique_ptr<T> resource, const char *name);

 private:
  FunctionBody *m_bodies[FunctionBody::BODY_TYPE_AMOUNT] = {0};

  StringRefNull m_name;

  Vector<StringRefNull> m_input_names;
  Vector<Type *> m_input_types;
  Vector<StringRefNull> m_output_names;
  Vector<Type *> m_output_types;

  std::mutex m_modify_mutex;
  std::unique_ptr<ResourceCollector> m_resources;
  const char *m_strings;
};

/* Function inline functions
 ***********************************************/

inline const StringRefNull Function::name() const
{
  return m_name;
}

#define STATIC_ASSERT_BODY_TYPE(T) \
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, T>::value), ""); \
  BLI_STATIC_ASSERT(T::FUNCTION_BODY_ID < FunctionBody::BODY_TYPE_AMOUNT, "")

template<typename T> inline bool Function::has_body() const
{
  STATIC_ASSERT_BODY_TYPE(T);
  BLI_STATIC_ASSERT((std::is_same<T, typename T::FunctionBodyType>::value), "");

  return m_bodies[T::FUNCTION_BODY_ID] != nullptr;
}

template<typename T> inline T &Function::body() const
{
  STATIC_ASSERT_BODY_TYPE(T);
  BLI_STATIC_ASSERT((std::is_same<T, typename T::FunctionBodyType>::value), "");
  BLI_assert(this->has_body<T>());

  FunctionBody *body_ptr = m_bodies[T::FUNCTION_BODY_ID];
  BLI_assert(dynamic_cast<T *>(body_ptr) == static_cast<T *>(body_ptr));
  return *static_cast<T *>(body_ptr);
}

template<typename T, typename... Args> inline T *Function::add_body(Args &&... args)
{
  STATIC_ASSERT_BODY_TYPE(T);
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, typename T::FunctionBodyType>::value), "")

  std::lock_guard<std::mutex> lock(m_modify_mutex);
  if (m_bodies[T::FUNCTION_BODY_ID] == nullptr) {
    T *new_body = new T(std::forward<Args>(args)...);
    new_body->set_owner(this);
    m_bodies[T::FUNCTION_BODY_ID] = dynamic_cast<FunctionBody *>(new_body);
    return new_body;
  }
  else {
    return nullptr;
  }
}

#undef STATIC_ASSERT_BODY_TYPE

inline bool operator==(const Function &a, const Function &b)
{
  return &a == &b;
}

inline uint Function::input_amount() const
{
  return m_input_names.size();
}

inline uint Function::output_amount() const
{
  return m_output_names.size();
}

inline Type *Function::input_type(uint index) const
{
  return m_input_types[index];
}

inline Type *Function::output_type(uint index) const
{
  return m_output_types[index];
}

inline StringRefNull Function::input_name(uint index) const
{
  return m_input_names[index];
}

inline StringRefNull Function::output_name(uint index) const
{
  return m_output_names[index];
}

template<typename T> inline Vector<T *> Function::input_extensions() const
{
  Vector<T *> extensions;
  for (auto &type : m_input_types) {
    extensions.append(&type->extension<T>());
  }
  return extensions;
}

template<typename T> inline Vector<T *> Function::output_extensions() const
{
  Vector<T *> extensions;
  for (auto &type : m_output_types) {
    extensions.append(&type->extension<T>());
  }
  return extensions;
}

inline ArrayRef<Type *> Function::input_types() const
{
  return m_input_types;
}

inline ArrayRef<Type *> Function::output_types() const
{
  return m_output_types;
}

template<typename T> void Function::add_resource(std::unique_ptr<T> resource, const char *name)
{
  std::lock_guard<std::mutex> lock(m_modify_mutex);

  if (m_resources.get() == nullptr) {
    m_resources = BLI::make_unique<ResourceCollector>();
  }

  m_resources->add(std::move(resource), name);
}

/* Function Body inline functions
 ********************************************/

inline void FunctionBody::set_owner(Function *fn)
{
  m_owner = fn;
  this->owner_init_post();
}

inline Function &FunctionBody::owner() const
{
  return *m_owner;
}

} /* namespace FN */
