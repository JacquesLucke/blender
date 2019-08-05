#include "generic_list.hpp"

namespace FN {

void GenericList::grow(uint min_capacity)
{
  if (m_capacity >= min_capacity) {
    return;
  }

  uint new_capacity = power_of_2_max_u(min_capacity);
  void *new_storage = MEM_malloc_arrayN(new_capacity, m_type_info->size(), __func__);
  m_type_info->relocate_to_uninitialized_n(m_storage, new_storage, m_size);

  if (m_storage != nullptr) {
    MEM_freeN(m_storage);
  }
  m_storage = new_storage;
  m_capacity = new_capacity;
}

}  // namespace FN
