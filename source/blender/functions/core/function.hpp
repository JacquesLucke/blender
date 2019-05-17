#pragma once

#include "signature.hpp"

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

  Function(StringRef name, const Signature &signature)
      : m_name(name.to_std_string()), m_signature(signature)
  {
  }

  Function(const Signature &signature) : Function("Function", signature)
  {
  }

  ~Function() = default;

  const StringRefNull name() const
  {
    return m_name;
  }

  inline const Signature &signature() const
  {
    return m_signature;
  }

  inline Signature &signature()
  {
    return m_signature;
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

  void print() const;

  /* Utility accessors */
  uint input_amount() const
  {
    return m_signature.inputs().size();
  }

  uint output_amount() const
  {
    return m_signature.outputs().size();
  }

  SharedType &input_type(uint index)
  {
    return m_signature.inputs()[index].type();
  }

  SharedType &output_type(uint index)
  {
    return m_signature.outputs()[index].type();
  }

  StringRefNull input_name(uint index)
  {
    return m_signature.inputs()[index].name();
  }

  StringRefNull output_name(uint index)
  {
    return m_signature.outputs()[index].name();
  }

  template<typename T> SmallVector<T *> input_extensions() const
  {
    SmallVector<T *> extensions;
    for (InputParameter &param : m_signature.inputs()) {
      T *ext = param.type()->extension<T>();
      BLI_assert(ext);
      extensions.append(ext);
    }
    return extensions;
  }

  template<typename T> SmallVector<T *> output_extensions() const
  {
    SmallVector<T *> extensions;
    for (OutputParameter &param : m_signature.outputs()) {
      T *ext = param.type()->extension<T>();
      BLI_assert(ext);
      extensions.append(ext);
    }
    return extensions;
  }

  SmallVector<SharedType> input_types() const
  {
    SmallVector<SharedType> types;
    for (InputParameter &param : m_signature.inputs()) {
      types.append(param.type());
    }
    return types;
  }

  SmallVector<SharedType> output_types() const
  {
    SmallVector<SharedType> types;
    for (OutputParameter &param : m_signature.outputs()) {
      types.append(param.type());
    }
    return types;
  }

 private:
  const std::string m_name;
  Signature m_signature;
  Composition m_bodies;
  mutable std::mutex m_body_mutex;
};

using SharedFunction = AutoRefCount<Function>;
using FunctionPerType = SmallMap<SharedType, SharedFunction>;

class FunctionBuilder {
 private:
  SmallVector<std::string> m_input_names;
  SmallVector<std::string> m_output_names;
  SmallVector<SharedType> m_input_types;
  SmallVector<SharedType> m_output_types;

 public:
  FunctionBuilder();
  void add_input(StringRef input_name, SharedType &type);
  void add_output(StringRef output_name, SharedType &type);

  SharedFunction build(StringRef function_name);
};

} /* namespace FN */
