#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"
#include "BLI_optional.h"
#include "BLI_virtual_list_ref.h"
#include "BLI_vector.h"
#include "BLI_utility_mixins.h"
#include "BLI_index_range.h"

#include "BKE_id_handle.h"

namespace FN {

using BKE::IDHandleLookup;
using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::Optional;
using BLI::Vector;
using BLI::VirtualListRef;

template<typename T> uintptr_t get_multi_function_element_context_id();

#define FN_MAKE_MF_ELEMENT_CONTEXT(name) \
  char name##_id_char = 0; \
  uintptr_t name##_id = (uintptr_t)&name##_id_char; \
  template<> uintptr_t get_multi_function_element_context_id<name>() \
  { \
    return name##_id; \
  }

class MFElementContexts {
 private:
  Vector<uintptr_t> m_ids;
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
    uintptr_t context_id = get_multi_function_element_context_id<T>();
    for (uint i : m_contexts.index_iterator()) {
      if (m_ids[i] == context_id) {
        const T *context = (const T *)m_contexts[i];
        return TypedContext<T>{context, m_indices[i]};
      }
    }
    return {};
  }
};

class MFContextBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFElementContexts m_element_contexts;
  const IDHandleLookup &m_id_handle_lookup;

  friend class MFContext;

 public:
  MFContextBuilder(const IDHandleLookup *id_handle_lookup = nullptr)
      : m_id_handle_lookup((id_handle_lookup == nullptr) ? IDHandleLookup::Empty() :
                                                           *id_handle_lookup)
  {
  }

  template<typename T> void add_element_context(const T &context, VirtualListRef<uint> indices)
  {
    m_element_contexts.m_ids.append(get_multi_function_element_context_id<T>());
    m_element_contexts.m_contexts.append((const void *)&context);
    m_element_contexts.m_indices.append(indices);
  }

  template<typename T> void add_element_context(const T &context, IndexRange indices)
  {
    this->add_element_context(context,
                              VirtualListRef<uint>::FromFullArray(indices.as_array_ref()));
  }

  template<typename T> void add_element_context(const T &context)
  {
    static uint dummy_index = 0;
    this->add_element_context(context, VirtualListRef<uint>::FromSingle_MaxSize(&dummy_index));
  }
};

class MFContext {
 private:
  MFContextBuilder *m_builder;

 public:
  MFContext(MFContextBuilder &builder) : m_builder(&builder)
  {
  }

  const IDHandleLookup &id_handle_lookup() const
  {
    return m_builder->m_id_handle_lookup;
  }

  const MFElementContexts &element_contexts() const
  {
    return m_builder->m_element_contexts;
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
