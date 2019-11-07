#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"
#include "BLI_optional.h"
#include "BLI_virtual_list_ref.h"
#include "BLI_vector.h"

namespace FN {

using BLI::ArrayRef;
using BLI::Optional;
using BLI::Vector;
using BLI::VirtualListRef;

class MFContext : BLI::NonCopyable, BLI::NonMovable {
 private:
  ArrayRef<const void *> m_context_ids;
  ArrayRef<const void *> m_context_data;
  ArrayRef<VirtualListRef<uint>> m_context_indices;

 public:
  MFContext(ArrayRef<const void *> context_ids,
            ArrayRef<const void *> context_data,
            ArrayRef<VirtualListRef<uint>> context_indices)
      : m_context_ids(context_ids),
        m_context_data(context_data),
        m_context_indices(context_indices)
  {
    BLI_assert(m_context_ids.size() == m_context_data.size());
    BLI_assert(m_context_ids.size() == m_context_indices.size());
  }

  struct ElementContext {
    const void *data;
    VirtualListRef<uint> indices;
  };

  Optional<ElementContext> try_find_context(const void *context_type_id) const
  {
    int index = m_context_ids.first_index_try(context_type_id);
    if (index >= 0) {
      return ElementContext{m_context_data[index], m_context_indices[index]};
    }
    else {
      return {};
    }
  }
};

class MFContextBuilder {
 private:
  Vector<const void *> m_context_ids;
  Vector<const void *> m_context_data;
  Vector<VirtualListRef<uint>> m_context_indices;
  MFContext m_context;

 public:
  MFContextBuilder() : m_context({}, {}, {})
  {
  }

  void add(const void *id, const void *data, VirtualListRef<uint> indices)
  {
    m_context_ids.append(id);
    m_context_data.append(data);
    m_context_indices.append(indices);
  }

  void add(const void *id, const void *data)
  {
    static uint dummy_index = 0;
    m_context_ids.append(id);
    m_context_data.append(data);
    m_context_indices.append(VirtualListRef<uint>::FromSingle_MaxSize(&dummy_index));
  }

  MFContext &build()
  {
    m_context.~MFContext();
    new (&m_context) MFContext(m_context_ids, m_context_data, m_context_indices);
    return m_context;
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
