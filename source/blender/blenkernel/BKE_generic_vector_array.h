#ifndef __BKE_GENERIC_MULTI_VECTOR_H__
#define __BKE_GENERIC_MULTI_VECTOR_H__

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"
#include "BKE_generic_array_ref.h"
#include "BKE_generic_virtual_list_list_ref.h"

#include "BLI_array_ref.h"
#include "BLI_index_range.h"
#include "BLI_monotonic_allocator.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::MonotonicAllocator;
using BLI::MutableArrayRef;

class GenericVectorArray : BLI::NonCopyable, BLI::NonMovable {
 private:
  BLI::GuardedAllocator m_slices_allocator;
  MonotonicAllocator<> m_elements_allocator;
  const CPPType &m_type;
  void **m_starts;
  uint *m_lengths;
  uint *m_capacities;
  uint m_array_size;
  uint m_element_size;

 public:
  GenericVectorArray() = delete;

  GenericVectorArray(const CPPType &type, uint array_size)
      : m_type(type), m_array_size(array_size), m_element_size(type.size())
  {
    uint byte_size__starts = sizeof(void *) * array_size;
    m_starts = (void **)m_slices_allocator.allocate(byte_size__starts, __func__);
    memset((void *)m_starts, 0, byte_size__starts);

    uint byte_size__lengths = sizeof(uint) * array_size;
    m_lengths = (uint *)m_slices_allocator.allocate(byte_size__lengths, __func__);
    memset((void *)m_lengths, 0, byte_size__lengths);

    uint byte_size__capacities = sizeof(uint) * array_size;
    m_capacities = (uint *)m_slices_allocator.allocate(byte_size__capacities, __func__);
    memset((void *)m_capacities, 0, byte_size__capacities);
  }

  ~GenericVectorArray()
  {
    this->destruct_all_elements();
    m_slices_allocator.deallocate((void *)m_starts);
    m_slices_allocator.deallocate((void *)m_lengths);
    m_slices_allocator.deallocate((void *)m_capacities);
  }

  uint size() const
  {
    return m_array_size;
  }

  const CPPType &type() const
  {
    return m_type;
  }

  const void *const *starts() const
  {
    return m_starts;
  }

  const uint *lengths() const
  {
    return m_lengths;
  }

  void append_single__copy(uint index, const void *src)
  {
    if (m_lengths[index] == m_capacities[index]) {
      this->grow_single(index, m_lengths[index] + 1);
    }

    void *dst = POINTER_OFFSET(m_starts[index], m_element_size * m_lengths[index]);
    m_type.copy_to_uninitialized(src, dst);
    m_lengths[index]++;
  }

  void extend_single__copy(uint index, const GenericVirtualListRef &values)
  {
    for (uint i = 0; i < values.size(); i++) {
      this->append_single__copy(index, values[i]);
    }
  }

  template<typename T> class TypedRef {
   private:
    const GenericVectorArray *m_data;

   public:
    TypedRef(const GenericVectorArray &data) : m_data(&data)
    {
    }

    ArrayRef<T> operator[](uint index) const
    {
      return ArrayRef<T>((const T *)m_data->m_starts[index], m_data->m_lengths[index]);
    }
  };

  template<typename T> class MutableTypedRef {
   private:
    GenericVectorArray *m_data;

   public:
    MutableTypedRef(GenericVectorArray &data) : m_data(&data)
    {
    }

    operator TypedRef<T>() const
    {
      return TypedRef<T>(*m_data);
    }

    MutableArrayRef<T> operator[](uint index) const
    {
      return MutableArrayRef<T>((T *)m_data->m_starts[index], m_data->m_lengths[index]);
    }

    void append_single(uint index, const T &value)
    {
      m_data->append_single__copy(index, (void *)&value);
    }
  };

  template<typename T> const TypedRef<T> as_typed_ref() const
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(m_type));
    return TypedRef<T>(*this);
  }

  template<typename T> MutableTypedRef<T> as_mutable_typed_ref()
  {
    BLI_assert(GET_TYPE<T>().is_same_or_generalization(m_type));
    return MutableTypedRef<T>(*this);
  }

 private:
  void grow_single(uint index, uint min_capacity)
  {
    BLI_assert(m_capacities[index] < min_capacity);
    min_capacity = power_of_2_max_u(min_capacity);
    void *new_buffer = m_elements_allocator.allocate_aligned(m_element_size * min_capacity,
                                                             m_type.alignment());

    for (uint i = 0; i < m_lengths[index]; i++) {
      void *src = POINTER_OFFSET(m_starts[index], m_element_size * i);
      void *dst = POINTER_OFFSET(new_buffer, m_element_size * i);
      m_type.relocate_to_uninitialized(src, dst);
    }

    m_starts[index] = new_buffer;
  }

  void destruct_all_elements()
  {
    if (m_type.trivially_destructible()) {
      return;
    }

    for (uint index = 0; index < m_array_size; index++) {
      for (uint i = 0; i < m_lengths[index]; i++) {
        void *ptr = POINTER_OFFSET(m_starts[index], m_element_size * i);
        m_type.destruct(ptr);
      }
    }
  }
};

};  // namespace BKE

#endif /* __BKE_GENERIC_MULTI_VECTOR_H__ */
