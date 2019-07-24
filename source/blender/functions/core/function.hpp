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

#include "type.hpp"
#include "BLI_chained_strings.hpp"

namespace FN {

class Function;

class FunctionBody {
 private:
  Function *m_owner = nullptr;

  void set_owner(Function *fn);

  friend class Function;

 protected:
  virtual void owner_init_post();

 public:
  FunctionBody() = default;
  FunctionBody(FunctionBody &other) = delete;
  FunctionBody(FunctionBody &&other) = delete;

  virtual ~FunctionBody();

  Function *owner() const;

  static const uint BODY_TYPE_AMOUNT = 5;
};

class Function final : public RefCountedBase {
 public:
  Function(Function &fn) = delete;

  /**
   * Construct a new function. Instead of calling this directly, the FunctionBuilder should be
   * used.
   */
  Function(ChainedStringRef name,
           ArrayRef<ChainedStringRef> input_names,
           ArrayRef<SharedType> input_types,
           ArrayRef<ChainedStringRef> output_names,
           ArrayRef<SharedType> output_types,
           const char *strings);

  ~Function();

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
   */
  template<typename T, typename... Args> bool add_body(Args &&... args);

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
  SharedType &input_type(uint index);

  /**
   * Get the type of the output at the given index.
   */
  SharedType &output_type(uint index);

  /**
   * Get the name of the input at the given index.
   */
  StringRefNull input_name(uint index);

  /**
   * Get the name of the output at the given index.
   */
  StringRefNull output_name(uint index);

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
  ArrayRef<SharedType> input_types() const;

  /**
   * Get an array containing all output types.
   */
  ArrayRef<SharedType> output_types() const;

  /**
   * Print some debug information for the function.
   */
  void print();

 private:
  ChainedStringRef m_name;
  std::mutex m_add_body_mutex;
  FunctionBody *m_bodies[FunctionBody::BODY_TYPE_AMOUNT] = {0};

  Vector<ChainedStringRef> m_input_names;
  Vector<SharedType> m_input_types;
  Vector<ChainedStringRef> m_output_names;
  Vector<SharedType> m_output_types;

  const char *m_strings;
};

using SharedFunction = AutoRefCount<Function>;

/* Function inline functions
 ***********************************************/

inline const StringRefNull Function::name() const
{
  return m_name.to_string_ref(m_strings);
}

#define STATIC_ASSERT_BODY_TYPE(T) \
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, T>::value), ""); \
  BLI_STATIC_ASSERT(T::FUNCTION_BODY_ID < FunctionBody::BODY_TYPE_AMOUNT, "")

template<typename T> inline bool Function::has_body() const
{
  STATIC_ASSERT_BODY_TYPE(T);
  return m_bodies[T::FUNCTION_BODY_ID] != nullptr;
}

template<typename T> inline T &Function::body() const
{
  STATIC_ASSERT_BODY_TYPE(T);
  BLI_assert(this->has_body<T>());
  return *(T *)m_bodies[T::FUNCTION_BODY_ID];
}

template<typename T, typename... Args> inline bool Function::add_body(Args &&... args)
{
  STATIC_ASSERT_BODY_TYPE(T);

  std::lock_guard<std::mutex> lock(m_add_body_mutex);
  if (m_bodies[T::FUNCTION_BODY_ID] == nullptr) {
    T *new_body = new T(std::forward<Args>(args)...);
    new_body->set_owner(this);
    m_bodies[T::FUNCTION_BODY_ID] = new_body;
    return true;
  }
  else {
    return false;
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

inline SharedType &Function::input_type(uint index)
{
  return m_input_types[index];
}

inline SharedType &Function::output_type(uint index)
{
  return m_output_types[index];
}

inline StringRefNull Function::input_name(uint index)
{
  return m_input_names[index].to_string_ref(m_strings);
}

inline StringRefNull Function::output_name(uint index)
{
  return m_output_names[index].to_string_ref(m_strings);
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

inline ArrayRef<SharedType> Function::input_types() const
{
  return m_input_types;
}

inline ArrayRef<SharedType> Function::output_types() const
{
  return m_output_types;
}

/* Function Body inline functions
 ********************************************/

inline void FunctionBody::set_owner(Function *fn)
{
  m_owner = fn;
  this->owner_init_post();
}

inline Function *FunctionBody::owner() const
{
  return m_owner;
}

} /* namespace FN */
