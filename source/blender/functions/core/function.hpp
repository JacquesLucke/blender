#pragma once

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

  Function(ChainedStringRef name,
           ArrayRef<ChainedStringRef> input_names,
           ArrayRef<SharedType> input_types,
           ArrayRef<ChainedStringRef> output_names,
           ArrayRef<SharedType> output_types,
           const char *strings);

  ~Function();

  const StringRefNull name() const;

  template<typename T> inline bool has_body() const;
  template<typename T> inline T *body() const;
  template<typename T, typename... Args> bool add_body(Args &&... args);

  void print();

  uint input_amount() const;
  uint output_amount() const;
  SharedType &input_type(uint index);
  SharedType &output_type(uint index);
  StringRefNull input_name(uint index);
  StringRefNull output_name(uint index);
  template<typename T> SmallVector<T *> input_extensions() const;
  template<typename T> SmallVector<T *> output_extensions() const;
  ArrayRef<SharedType> input_types() const;
  ArrayRef<SharedType> output_types() const;

 private:
  ChainedStringRef m_name;
  Composition m_bodies;
  mutable std::mutex m_body_mutex;

  SmallVector<ChainedStringRef> m_input_names;
  SmallVector<SharedType> m_input_types;
  SmallVector<ChainedStringRef> m_output_names;
  SmallVector<SharedType> m_output_types;

  const char *m_strings;
};

using SharedFunction = AutoRefCount<Function>;

class FunctionBuilder {
 private:
  ChainedStringsBuilder m_strings_builder;
  SmallVector<ChainedStringRef> m_input_names;
  SmallVector<SharedType> m_input_types;
  SmallVector<ChainedStringRef> m_output_names;
  SmallVector<SharedType> m_output_types;

 public:
  FunctionBuilder();
  void add_input(StringRef input_name, SharedType &type);
  void add_output(StringRef output_name, SharedType &type);

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
  static_assert(std::is_base_of<FunctionBody, T>::value, "");
  return this->m_bodies.has<T>();
}

template<typename T> inline T *Function::body() const
{
  std::lock_guard<std::mutex> lock(m_body_mutex);
  static_assert(std::is_base_of<FunctionBody, T>::value, "");
  return m_bodies.get<T>();
}

template<typename T, typename... Args> inline bool Function::add_body(Args &&... args)
{
  std::lock_guard<std::mutex> lock(m_body_mutex);
  static_assert(std::is_base_of<FunctionBody, T>::value, "");

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

template<typename T> inline SmallVector<T *> Function::input_extensions() const
{
  SmallVector<T *> extensions;
  for (auto &type : m_input_types) {
    T *ext = type->extension<T>();
    BLI_assert(ext);
    extensions.append(ext);
  }
  return extensions;
}

template<typename T> inline SmallVector<T *> Function::output_extensions() const
{
  SmallVector<T *> extensions;
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
