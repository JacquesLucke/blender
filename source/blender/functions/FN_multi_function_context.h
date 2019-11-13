#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"
#include "BLI_optional.h"
#include "BLI_virtual_list_ref.h"
#include "BLI_vector.h"
#include "BLI_utility_mixins.h"
#include "BLI_index_range.h"

namespace FN {

using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::Optional;
using BLI::Vector;
using BLI::VirtualListRef;

class MFElementContext {
 public:
  virtual ~MFElementContext();
};

class MFElementContexts {
 private:
  Vector<const MFElementContext *> m_contexts;
  Vector<VirtualListRef<uint>> m_indices;

  friend class MFContextBuilder;

 public:
  MFElementContexts() = default;

  template<typename T> struct TypedContext {
    const T *data;
    VirtualListRef<uint> indices;
  };

  template<typename T> Optional<TypedContext<T>> find_first() const
  {
    BLI_STATIC_ASSERT((std::is_base_of<MFElementContext, T>::value), "");
    for (uint i = 0; i < m_contexts.size(); i++) {
      const T *context = dynamic_cast<const T *>(m_contexts[i]);
      if (context != nullptr) {
        return TypedContext<T>{context, m_indices[i]};
      }
    }
    return {};
  }
};

class MFContextBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFElementContexts m_element_contexts;

  friend class MFContext;

 public:
  MFContextBuilder()
  {
  }

  void add_element_context(const MFElementContext &context, VirtualListRef<uint> indices)
  {
    m_element_contexts.m_contexts.append(&context);
    m_element_contexts.m_indices.append(indices);
  }

  void add_element_context(const MFElementContext &context, IndexRange indices)
  {
    this->add_element_context(context,
                              VirtualListRef<uint>::FromFullArray(indices.as_array_ref()));
  }

  void add_element_context(const MFElementContext &context)
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

  const MFElementContexts &element_contexts() const
  {
    return m_builder->m_element_contexts;
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
