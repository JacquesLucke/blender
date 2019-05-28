#pragma once

#include "type.hpp"
#include "BLI_chained_strings.hpp"

namespace FN {

class Function;

class FunctionBody {
 private:
  Function *m_owner = nullptr;

  void set_owner(Function *fn)
  {
    m_owner = fn;
    this->owner_init_post();
  }

  friend class Function;

 protected:
  virtual void owner_init_post()
  {
  }

 public:
  virtual ~FunctionBody()
  {
  }

  Function *owner() const
  {
    return m_owner;
  }
};

class Function final : public RefCountedBase {
 public:
  Function(Function &fn) = delete;

  Function(ChainedStringRef name,
           ArrayRef<ChainedStringRef> input_names,
           ArrayRef<SharedType> input_types,
           ArrayRef<ChainedStringRef> output_names,
           ArrayRef<SharedType> output_types,
           const char *strings)
      : m_name(name),
        m_input_names(input_names.to_small_vector()),
        m_input_types(input_types.to_small_vector()),
        m_output_names(output_names.to_small_vector()),
        m_output_types(output_types.to_small_vector()),
        m_strings(strings)
  {
    BLI_assert(m_input_names.size() == m_input_types.size());
    BLI_assert(m_output_names.size() == m_output_types.size());
  }

  ~Function();

  const StringRefNull name() const
  {
    return m_name.to_string_ref(m_strings);
  }

  template<typename T> inline bool has_body() const
  {
    std::lock_guard<std::mutex> lock(m_body_mutex);
    static_assert(std::is_base_of<FunctionBody, T>::value, "");
    return this->m_bodies.has<T>();
  }

  template<typename T> inline T *body() const
  {
    std::lock_guard<std::mutex> lock(m_body_mutex);
    static_assert(std::is_base_of<FunctionBody, T>::value, "");
    return m_bodies.get<T>();
  }

  template<typename T, typename... Args> bool add_body(Args &&... args)
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

  friend bool operator==(const Function &a, const Function &b)
  {
    return &a == &b;
  }

  void print();

  /* Utility accessors */
  uint input_amount() const
  {
    return m_input_names.size();
  }

  uint output_amount() const
  {
    return m_output_names.size();
  }

  SharedType &input_type(uint index)
  {
    return m_input_types[index];
  }

  SharedType &output_type(uint index)
  {
    return m_output_types[index];
  }

  StringRefNull input_name(uint index)
  {
    return m_input_names[index].to_string_ref(m_strings);
  }

  StringRefNull output_name(uint index)
  {
    return m_output_names[index].to_string_ref(m_strings);
  }

  template<typename T> SmallVector<T *> input_extensions() const
  {
    SmallVector<T *> extensions;
    for (auto &type : m_input_types) {
      T *ext = type->extension<T>();
      BLI_assert(ext);
      extensions.append(ext);
    }
    return extensions;
  }

  template<typename T> SmallVector<T *> output_extensions() const
  {
    SmallVector<T *> extensions;
    for (auto &type : m_output_types) {
      T *ext = type->extension<T>();
      BLI_assert(ext);
      extensions.append(ext);
    }
    return extensions;
  }

  ArrayRef<SharedType> input_types() const
  {
    return m_input_types;
  }

  ArrayRef<SharedType> output_types() const
  {
    return m_output_types;
  }

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
using FunctionPerType = SmallMap<SharedType, SharedFunction>;

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

} /* namespace FN */
