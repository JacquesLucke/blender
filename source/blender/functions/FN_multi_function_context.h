#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"
#include "BLI_optional.h"
#include "BLI_virtual_list_ref.h"
#include "BLI_vector.h"
#include "BLI_utility_mixins.h"

namespace FN {

using BLI::ArrayRef;
using BLI::Optional;
using BLI::Vector;
using BLI::VirtualListRef;

class MFElementContext {
 public:
  virtual ~MFElementContext();
};

class MFElementContexts {
 private:
  ArrayRef<const MFElementContext *> m_contexts;
  ArrayRef<VirtualListRef<uint>> m_indices;

 public:
  MFElementContexts() = default;

  MFElementContexts(ArrayRef<const MFElementContext *> contexts,
                    ArrayRef<VirtualListRef<uint>> indices)
      : m_contexts(contexts), m_indices(indices)
  {
    BLI_assert(contexts.size() == indices.size());
  }

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

class MFContext : BLI::NonCopyable, BLI::NonMovable {
 private:
  MFElementContexts m_element_contexts;

 public:
  MFContext() = default;
  MFContext(MFElementContexts element_contexts) : m_element_contexts(element_contexts)
  {
  }

  const MFElementContexts &element_contexts() const
  {
    return m_element_contexts;
  }
};

class MFContextBuilder {
 private:
  Vector<const MFElementContext *> m_element_contexts;
  Vector<VirtualListRef<uint>> m_element_context_indices;
  MFContext m_context;

 public:
  MFContextBuilder()
  {
  }

  void add_element_context(const MFElementContext *context, VirtualListRef<uint> indices)
  {
    m_element_contexts.append(context);
    m_element_context_indices.append(indices);
  }

  void add_element_context(const MFElementContext *context)
  {
    static uint dummy_index = 0;
    this->add_element_context(context, VirtualListRef<uint>::FromSingle_MaxSize(&dummy_index));
  }

  MFContext &build()
  {
    m_context.~MFContext();
    new (&m_context) MFContext(MFElementContexts(m_element_contexts, m_element_context_indices));
    return m_context;
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
