#ifndef __BKE_GENERIC_MULTI_VECTOR_H__
#define __BKE_GENERIC_MULTI_VECTOR_H__

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"
#include "BKE_generic_array_ref.h"

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
  struct BufferSlice {
    void *start;
    uint length;
    uint capacity;
  };

  BLI::GuardedAllocator m_slices_allocator;
  MonotonicAllocator<> m_elements_allocator;
  CPPType &m_type;
  BufferSlice *m_slices;
  uint m_array_size;
  uint m_element_size;

 public:
  GenericVectorArray() = delete;

  GenericVectorArray(CPPType &type, uint array_size)
      : m_type(type), m_array_size(array_size), m_element_size(type.size())
  {
    uint slices_size_in_bytes = sizeof(BufferSlice) * array_size;
    m_slices = (BufferSlice *)m_slices_allocator.allocate(slices_size_in_bytes, __func__);
    memset((void *)m_slices, 0, slices_size_in_bytes);
  }

  ~GenericVectorArray()
  {
    this->destruct_all_elements();
    m_slices_allocator.deallocate((void *)m_slices);
  }

  void append_single__copy(uint index, const void *src)
  {
    MutableArrayRef<BufferSlice> slices = this->slices();
    BufferSlice &slice = slices[index];
    if (slice.length == slice.capacity) {
      this->grow_single(slice, slice.length + 1);
    }

    void *dst = POINTER_OFFSET(slice.start, m_element_size * slice.length);
    m_type.copy_to_uninitialized(src, dst);
    slice.length++;
  }

  void append_all__copy(const GenericArrayRef &array)
  {
    BLI_assert(m_array_size == array.size());

    for (BufferSlice &slice : this->slices()) {
      if (slice.length == slice.capacity) {
        this->grow_single(slice, slice.length + 1);
      }
    }

    const void *src_buffer = array.buffer();
    for (BufferSlice &slice : this->slices()) {
      void *dst = POINTER_OFFSET(slice.start, m_element_size * slice.length);
      m_type.copy_to_uninitialized(src_buffer, dst);
      slice.length++;

      src_buffer = POINTER_OFFSET(src_buffer, m_element_size);
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
      const BufferSlice &slice = m_data->slices()[index];
      return ArrayRef<T>((const T *)slice.start, slice.length);
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
      const BufferSlice &slice = m_data->slices()[index];
      return MutableArrayRef<T>((T *)slice.start, slice.length);
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

 private:
  void grow_single(BufferSlice &slice, uint min_capacity)
  {
    BLI_assert(slice.capacity < min_capacity);
    min_capacity = power_of_2_max_u(min_capacity);
    void *new_buffer = m_elements_allocator.allocate_aligned(m_element_size * min_capacity,
                                                             m_type.alignment());

    for (uint i = 0; i < slice.length; i++) {
      void *src = POINTER_OFFSET(slice.start, m_element_size * i);
      void *dst = POINTER_OFFSET(new_buffer, m_element_size * i);
      m_type.relocate_to_uninitialized(src, dst);
    }

    slice.start = new_buffer;
  }

  void destruct_all_elements()
  {
    if (m_type.trivially_destructible()) {
      return;
    }

    for (const BufferSlice &slice : this->slices()) {
      for (uint i = 0; i < slice.length; i++) {
        void *ptr = POINTER_OFFSET(slice.start, m_element_size * i);
        m_type.destruct(ptr);
      }
    }
  }

  ArrayRef<BufferSlice> slices() const
  {
    return ArrayRef<BufferSlice>(m_slices, m_array_size);
  }

  MutableArrayRef<BufferSlice> slices()
  {
    return MutableArrayRef<BufferSlice>(m_slices, m_array_size);
  }
};

};  // namespace BKE

#endif /* __BKE_GENERIC_MULTI_VECTOR_H__ */