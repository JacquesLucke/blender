#pragma once

#include "tuple.hpp"
#include "BLI_refcount.hpp"

namespace FN {

class GenericList;
using SharedGenericList = AutoRefCount<GenericList>;

class GenericList : public RefCounter {
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

  GenericList(GenericList &other) = delete;
  GenericList(GenericList &&other) = delete;
  GenericList &operator=(GenericList &other) = delete;
  GenericList &operator=(GenericList &&other) = delete;

  ~GenericList()
  {
    if (m_storage != nullptr) {
      m_type_info->destruct_n(m_storage, m_size);
      MEM_freeN(m_storage);
    }
  }

  bool is_mutable()
  {
    return this->refcount() == 1;
  }

  SharedGenericList get_mutable()
  {
    if (this->is_mutable()) {
      return SharedGenericList(this);
    }
    else {
      return this->real_copy();
    }
  }

  SharedGenericList real_copy()
  {
    GenericList *list = new GenericList(m_type);
    SharedGenericList this_list(this);
    list->extend__dynamic_copy(this_list);
    this_list.extract_ptr();
    return SharedGenericList(list);
  }

  void append__dynamic_relocate_from_tuple(Tuple &tuple, uint index)
  {
    BLI_assert(this->is_mutable());
    BLI_assert(&tuple.meta().type_info(index) == m_type_info);
    this->ensure_space_for_one();
    void *dst = POINTER_OFFSET(m_storage, m_size * m_type_info->size());
    tuple.relocate_out__dynamic(index, dst);
    m_size++;
  }

  void get__dynamic_copy_to_tuple(uint element_index, Tuple &tuple, uint tuple_index)
  {
    BLI_assert(this->is_mutable());
    BLI_assert(&tuple.meta().type_info(tuple_index) == m_type_info);
    BLI_assert(element_index < m_size);
    void *src = POINTER_OFFSET(m_storage, element_index * m_type_info->size());
    tuple.copy_in__dynamic(tuple_index, src);
  }

  void extend__dynamic_copy(const SharedGenericList &other)
  {
    BLI_assert(this->is_mutable());
    BLI_assert(m_type == other->m_type);
    GenericList &other_ = other.ref();
    this->reserve(m_size + other_.size());
    void *src = other_.m_storage;
    void *dst = POINTER_OFFSET(m_storage, m_size * m_type_info->size());
    m_type_info->copy_to_uninitialized_n(src, dst, other_.size());
    m_size += other_.size();
  }

  void *storage() const
  {
    return m_storage;
  }

  template<typename T> T *storage() const
  {
    BLI_assert(this->can_be_type<T>());
    return static_cast<T *>(m_storage);
  }

  template<typename T> ArrayRef<T> as_array_ref() const
  {
    BLI_assert(this->can_be_type<T>());
    return ArrayRef<T>(this->storage<T>(), m_size);
  }

  template<typename T> bool can_be_type() const
  {
    return sizeof(T) == m_type_info->size();
  }

  uint size() const
  {
    return m_size;
  }

  SharedType &type()
  {
    return m_type;
  }

  void reserve_and_set_size(uint size)
  {
    this->reserve(size);
    m_size = size;
  }

  void reserve(uint size)
  {
    BLI_assert(this->is_mutable());
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

  void grow(uint min_capacity);
};

}  // namespace FN
