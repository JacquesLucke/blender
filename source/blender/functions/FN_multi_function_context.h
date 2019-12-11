#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"
#include "BLI_optional.h"
#include "BLI_virtual_list_ref.h"
#include "BLI_vector.h"
#include "BLI_utility_mixins.h"
#include "BLI_index_range.h"
#include "BLI_static_class_ids.h"

#include "BKE_id_handle.h"

namespace FN {

using BKE::IDHandleLookup;
using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::Optional;
using BLI::Vector;
using BLI::VirtualListRef;

class MFElementContexts {
 private:
  Vector<BLI::class_id_t> m_ids;
  Vector<const void *> m_contexts;
  Vector<VirtualListRef<uint>> m_indices;

  friend class MFContextBuilder;

 public:
  MFElementContexts() = default;

  template<typename T> struct TypedContext {
    const T *data;
    VirtualListRef<uint> indices;
  };

  template<typename T> Optional<TypedContext<T>> try_find() const
  {
    BLI::class_id_t context_id = BLI::get_class_id<T>();
    for (uint i : m_contexts.index_iterator()) {
      if (m_ids[i] == context_id) {
        const T *context = (const T *)m_contexts[i];
        return TypedContext<T>{context, m_indices[i]};
      }
    }
    return {};
  }
};

class MFGlobalContexts {
 private:
  Vector<BLI::class_id_t> m_ids;
  Vector<const void *> m_contexts;

  friend class MFContextBuilder;

 public:
  MFGlobalContexts() = default;

  template<typename T> const T *try_find() const
  {
    BLI::class_id_t context_id = BLI::get_class_id<T>();
    for (uint i : m_contexts.index_iterator()) {
      if (m_ids[i] == context_id) {
        const T *context = (const T *)m_contexts[i];
        return context;
      }
    }
    return nullptr;
  }
};

class MFContextBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFElementContexts m_element_contexts;
  MFGlobalContexts m_global_contexts;

  friend class MFContext;

 public:
  MFContextBuilder()
  {
  }

  template<typename T> void add_element_context(const T &context, VirtualListRef<uint> indices)
  {
    m_element_contexts.m_ids.append(BLI::get_class_id<T>());
    m_element_contexts.m_contexts.append((const void *)&context);
    m_element_contexts.m_indices.append(indices);
  }

  template<typename T> void add_element_context(const T &context, IndexRange indices)
  {
    this->add_element_context(context,
                              VirtualListRef<uint>::FromFullArray(indices.as_array_ref()));
  }

  template<typename T> void add_global_context(const T &context)
  {
    m_global_contexts.m_ids.append(BLI::get_class_id<T>());
    m_global_contexts.m_contexts.append((const void *)&context);
  }
};

class MFContext {
 private:
  MFContextBuilder *m_builder;

 public:
  MFContext(MFContextBuilder &builder) : m_builder(&builder)
  {
  }

  template<typename T> Optional<MFElementContexts::TypedContext<T>> try_find_per_element() const
  {
    return m_builder->m_element_contexts.try_find<T>();
  }

  template<typename T> const T *try_find_global() const
  {
    return m_builder->m_global_contexts.try_find<T>();
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
