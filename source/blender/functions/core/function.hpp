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
  virtual ~FunctionBody();

  Function *owner() const;
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
   * Return a type extension of type T if it exists in the function. Otherwise nullptr.
   */
  template<typename T> inline T *body() const;

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
  Composition m_bodies;
  mutable std::mutex m_body_mutex;

  Vector<ChainedStringRef> m_input_names;
  Vector<SharedType> m_input_types;
  Vector<ChainedStringRef> m_output_names;
  Vector<SharedType> m_output_types;

  const char *m_strings;
};

using SharedFunction = AutoRefCount<Function>;

class FunctionBuilder {
 private:
  ChainedStringsBuilder m_strings_builder;
  Vector<ChainedStringRef> m_input_names;
  Vector<SharedType> m_input_types;
  Vector<ChainedStringRef> m_output_names;
  Vector<SharedType> m_output_types;

 public:
  FunctionBuilder();

  /**
   * Add an input to the function with the given name and type.
   */
  void add_input(StringRef input_name, SharedType &type);

  /**
   * Add an output to the function with the given name and type.
   */
  void add_output(StringRef output_name, SharedType &type);

  /**
   * Create a new function with the given name and all the inputs and outputs previously added.
   */
  SharedFunction build(StringRef function_name);
};

/* Function inline functions
 ***********************************************/

inline const StringRefNull Function::name() const
{
  return m_name.to_string_ref(m_strings);
}

template<typename T> inline bool Function::has_body() const
{
  std::lock_guard<std::mutex> lock(m_body_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, T>::value), "");
  return this->m_bodies.has<T>();
}

template<typename T> inline T *Function::body() const
{
  std::lock_guard<std::mutex> lock(m_body_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, T>::value), "");
  return m_bodies.get<T>();
}

template<typename T, typename... Args> inline bool Function::add_body(Args &&... args)
{
  std::lock_guard<std::mutex> lock(m_body_mutex);
  BLI_STATIC_ASSERT((std::is_base_of<FunctionBody, T>::value), "");

  if (m_bodies.has<T>()) {
    return false;
  }
  else {
    T *new_body = new T(std::forward<Args>(args)...);
    new_body->set_owner(this);
    m_bodies.add(new_body);
    return true;
  }
}

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
    T *ext = type->extension<T>();
    BLI_assert(ext);
    extensions.append(ext);
  }
  return extensions;
}

template<typename T> inline Vector<T *> Function::output_extensions() const
{
  Vector<T *> extensions;
  for (auto &type : m_output_types) {
    T *ext = type->extension<T>();
    BLI_assert(ext);
    extensions.append(ext);
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
