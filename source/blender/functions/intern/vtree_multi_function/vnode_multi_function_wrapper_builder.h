#pragma once

#include "vnode_multi_function_wrapper.h"
#include "vsocket_multi_function_data_types.h"
#include "mappings.h"

#include "BLI_resource_collector.h"

namespace FN {

using BLI::ResourceCollector;

struct VNodeMFWrapperBuilderGlobals {
  ResourceCollector &resources;
  const VirtualNodeTree &vtree;
  const VSocketMFDataTypes &vsocket_data_types;
  const VTreeMFMappings &mappings;
};

class VNodeMFWrapperBuilder {
 private:
  VNodeMFWrapperBuilderGlobals &m_globals;

  const VNode &m_vnode_to_wrap;
  ArrayRef<const VOutputSocket *> m_outputs_to_compute;
  VNodeMFWrapper &m_wrapper_to_build;

 public:
  VNodeMFWrapperBuilder(VNodeMFWrapperBuilderGlobals &globals,
                        const VNode &vnode_to_wrap,
                        ArrayRef<const VOutputSocket *> outputs_to_compute,
                        VNodeMFWrapper &wrapper_to_build)
      : m_globals(globals),
        m_vnode_to_wrap(vnode_to_wrap),
        m_outputs_to_compute(outputs_to_compute),
        m_wrapper_to_build(wrapper_to_build)
  {
    BLI_assert(!m_outputs_to_compute.contains(nullptr));
    BLI_assert(!m_outputs_to_compute.has_duplicates__linear_search());
  }

  const VNode &vnode() const
  {
    return m_vnode_to_wrap;
  }

  ArrayRef<const VOutputSocket *> outputs_to_compute() const
  {
    return m_outputs_to_compute;
  }

  bool output_is_required(const VOutputSocket &vsocket) const
  {
    return m_outputs_to_compute.contains(&vsocket);
  }

  const CPPType &cpp_type_from_property(StringRefNull prop_name)
  {
    char *type_name = RNA_string_get_alloc(m_vnode_to_wrap.rna(), prop_name.data(), nullptr, 0);
    const CPPType &type = *m_globals.mappings.cpp_type_by_name.lookup(type_name);
    MEM_freeN(type_name);
    return type;
  }

  MFDataType data_type_from_property(StringRefNull prop_name)
  {
    char *type_name = RNA_string_get_alloc(m_vnode_to_wrap.rna(), prop_name.data(), nullptr, 0);
    MFDataType type = m_globals.mappings.data_type_by_name.lookup(type_name);
    MEM_freeN(type_name);
    return type;
  }

  Vector<bool> get_list_base_variadic_states(StringRefNull prop_name);

  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<MultiFunction, T>::value), "");
    void *buffer = m_globals.resources.allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_globals.resources.add(BLI::destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }

  const MultiFunction &get_vectorized_function(const MultiFunction &base_function,
                                               ArrayRef<const char *> is_vectorized_prop_names);

  template<typename T, typename... Args>
  void set_vectorized_constructed_matching_fn(ArrayRef<const char *> is_vectorized_prop_names,
                                              Args &&... args)
  {
    const MultiFunction &base_fn = this->construct_fn<T>(std::forward<Args>(args)...);
    const MultiFunction &fn = this->get_vectorized_function(base_fn, is_vectorized_prop_names);
    this->set_matching_fn(fn);
  }

  template<typename T, typename... Args> void set_constructed_matching_fn(Args &&... args)
  {
    const MultiFunction &fn = this->construct_fn<T>(std::forward<Args>(args)...);
    this->set_matching_fn(fn);
  }

  void set_matching_fn(const MultiFunction &fn);
  void set_fn(const MultiFunction &fn, Vector<VSocketsForMFParam> param_vsockets);

 private:
  void assert_valid_param_vsockets(const MultiFunction &fn,
                                   ArrayRef<VSocketsForMFParam> param_vsockets);
};

}  // namespace FN
