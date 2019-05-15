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

 private:
  const std::string m_name;
  Signature m_signature;
  Composition m_bodies;
  mutable std::mutex m_body_mutex;
};

using SharedFunction = AutoRefCount<Function>;
using FunctionPerType = SmallMap<SharedType, SharedFunction>;

} /* namespace FN */
