#include "BKE_tuple.h"

namespace BKE {

TupleInfo::TupleInfo(Vector<const CPPType *> types) : m_types(std::move(types))
{
  m_all_trivially_destructible = true;
  m_size__data = 0;
  m_alignment = 1;
  for (const CPPType *type : m_types) {
    uint size = type->size();
    uint alignment = type->alignment();

    m_alignment = std::max(m_alignment, alignment);

    m_size__data = pad_up(m_size__data, alignment);
    m_offsets.append(m_size__data);
    m_size__data += size;

    if (!type->trivially_destructible()) {
      m_all_trivially_destructible = false;
    }
  }

  m_do_align_mask = ~(uintptr_t)(m_alignment - 1);
  m_size__data_and_init = m_size__data + m_types.size();
  m_size__alignable_data_and_init = m_size__data_and_init + m_alignment - 1;
}

}  // namespace BKE
