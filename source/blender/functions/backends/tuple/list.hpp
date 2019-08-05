#pragma once

#include "tuple.hpp"
#include "BLI_shared_immutable.hpp"

namespace FN {

class GenericList {
 private:
  SharedType m_type;
  CPPTypeInfo *m_type_info;
  void *m_storage;
  uint m_size;
  uint m_capacity;

 public:
  GenericList() = delete;
  GenericList(SharedType type) : m_type(std::move(type))
  {
    m_type_info = &m_type->extension<CPPTypeInfo>();
    m_storage = nullptr;
    m_size = 0;
    m_capacity = 0;
  }

  GenericList(const GenericList &other) : m_type(other.m_type), m_type_info(other.m_type_info)
  {
    m_size = other.m_size;
    m_capacity = m_size;
    m_storage = MEM_malloc_arrayN(m_size, m_type_info->size(), __func__);
    m_type_info->copy_to_uninitialized_n(other.m_storage, m_storage, m_size);
  }

  GenericList(GenericList &&other)
      : m_type(std::move(other.m_type)),
        m_type_info(other.m_type_info),
        m_storage(other.m_storage),
        m_size(other.m_size),
        m_capacity(other.m_capacity)
  {
    other.m_storage = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
  }

  ~GenericList()
  {
    if (m_storage != nullptr) {
      m_type_info->destruct_n(m_storage, m_size);
      MEM_freeN(m_storage);
    }
  }

  GenericList &operator=(const GenericList &other)
  {
    if (this == &other) {
      return *this;
    }

    delete this;
    new (this) GenericList(other);

    return *this;
  }

  GenericList &operator=(GenericList &&other)
  {
    if (this == &other) {
      return *this;
    }

    delete this;
    new (this) GenericList(std::move(other));

    return *this;
  }

  void append__dynamic_relocate_from_tuple(Tuple &tuple, uint index)
  {
    BLI_assert(&tuple.meta().type_info(index) == m_type_info);
    this->ensure_space_for_one();
    void *dst = POINTER_OFFSET(m_storage, m_size * m_type_info->size());
    tuple.relocate_out__dynamic(index, dst);
    m_size++;
  }

  void get__dynamic_copy_to_tuple(uint element_index, Tuple &tuple, uint tuple_index)
  {
    BLI_assert(&tuple.meta().type_info(tuple_index) == m_type_info);
    BLI_assert(element_index < m_size);
    void *src = POINTER_OFFSET(m_storage, element_index * m_type_info->size());
    tuple.copy_in__dynamic(tuple_index, src);
  }

  void extend__dynamic_copy(const GenericList &other)
  {
    BLI_assert(m_type == other.m_type);
    this->reserve(m_size + other.size());
    void *src = other.m_storage;
    void *dst = POINTER_OFFSET(m_storage, m_size * m_type_info->size());
    m_type_info->copy_to_uninitialized_n(src, dst, other.size());
  }

  void *storage() const
  {
    return m_storage;
  }

  uint size() const
  {
    return m_size;
  }

  SharedType &type()
  {
    return m_type;
  }

  void reserve(uint size)
  {
    if (size > m_capacity) {
      this->grow(size);
    }
  }

 private:
  void ensure_space_for_one()
  {
    if (m_size < m_capacity) {
      return;
    }
    this->grow(m_capacity + 1);
  }

  void grow(uint min_capacity)
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
};

}  // namespace FN
