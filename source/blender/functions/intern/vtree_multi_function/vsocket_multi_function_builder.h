#pragma once

#include "BLI_resource_collector.h"

#include "BKE_virtual_node_tree.h"

#include "FN_multi_function.h"
#include "FN_multi_functions.h"

namespace FN {

using BKE::VSocket;
using BLI::ResourceCollector;

class VSocketMFBuilder {
 private:
  ResourceCollector &m_resources;

  const VSocket &m_vsocket;
  const MultiFunction *m_function = nullptr;

 public:
  VSocketMFBuilder(ResourceCollector &resources, const VSocket &vsocket)
      : m_resources(resources), m_vsocket(vsocket)
  {
  }

  const VSocket &vsocket() const
  {
    return m_vsocket;
  }

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<MultiFunction, T>::value), "");
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }

  template<typename T> void build_constant_value_fn(const T &value)
  {
    const MultiFunction &fn = this->construct_fn<MF_ConstantValue<T>>(value);
    this->set_fn(fn);
  }

  void set_fn(const MultiFunction &fn)
  {
    m_function = &fn;
  }

  const MultiFunction &get_built_function() const
  {
    BLI_assert(m_function != nullptr);
    return *m_function;
  }
};

}  // namespace FN
