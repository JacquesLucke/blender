#ifndef __BKE_TUPLE_H__
#define __BKE_TUPLE_H__

#include "BLI_vector.h"

#include "BKE_cpp_type.h"
#include "BKE_cpp_types.h"

namespace BKE {

using BLI::ArrayRef;
using BLI::Vector;

class TupleInfo : BLI::NonCopyable, BLI::NonMovable {
 private:
  Vector<uint> m_offsets;
  Vector<CPPType *> m_types;
  uint m_alignment;
  uintptr_t m_do_align_mask;
  uint m_size__data;
  uint m_size__data_and_init;
  uint m_size__alignable_data_and_init;
  bool m_all_trivially_destructible;

 public:
  TupleInfo(Vector<CPPType *> types);

  ArrayRef<CPPType *> types() const
  {
    return m_types;
  }

  CPPType &type_at_index(uint index) const
  {
    return *m_types[index];
  }

  uint offset_of_index(uint index) const
  {
    return m_offsets[index];
  }

  uint size_of_data() const
  {
    return m_size__data;
  }

  uint size_of_init() const
  {
    return m_size__data_and_init - m_size__data;
  }

  uint size_of_data_and_init() const
  {
    return m_size__data_and_init;
  }

  uint size_of_alignable_data_and_init() const
  {
    return m_size__alignable_data_and_init;
  }

  void *align_data_buffer(void *ptr) const
  {
    uintptr_t ptr_i = (uintptr_t)ptr;
    uintptr_t aligned_ptr_i = ptr_i & m_do_align_mask;
    void *aligned_ptr = (void *)aligned_ptr_i;
    return aligned_ptr;
  }

  uint size() const
  {
    return m_types.size();
  }

  uint alignment() const
  {
    return m_alignment;
  }

  bool all_trivially_destructible() const
  {
    return m_all_trivially_destructible;
  }

  template<typename T> bool element_has_type(uint index) const
  {
    CPPType *expected_type = m_types[index];
    CPPType *actual_type == get_cpp_type<T>();
    return expected_type == actual_type;
  }
};

class TupleRef {
 private:
  TupleInfo *m_info;
  void *m_data;
  bool *m_init;

  TupleRef(TupleInfo &info, void *data, bool *init) : m_info(&info), m_data(data), m_init(init)
  {
    BLI_assert(m_info != nullptr);
    BLI_assert(m_data != nullptr);
    BLI_assert(m_init != nullptr);
    BLI_assert(POINTER_AS_UINT(data) % m_info->alignment() == 0);
  }

 public:
  static TupleRef FromPreparedBuffers(TupleInfo &info, void *data, bool *init)
  {
    return TupleRef(info, data, init);
  }

  static TupleRef FromAlignableBuffer(TupleInfo &info, void *alignable_buffer)
  {
    void *data = info.align_data_buffer(alignable_buffer);
    bool *init = (bool *)POINTER_OFFSET(data, info.size_of_data());
    return TupleRef(info, data, init);
  }

  ~TupleRef() = default;

  template<typename T> void copy_in(uint index, const T &value)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_info->element_has_type<T>(index));

    T *dst = (T *)this->element_ptr(index);
    if (std::is_trivially_copyable<T>::value) {
      std::memcpy(dst, &value, sizeof(T));
    }
    else {
      if (m_init[index]) {
        *dst = value;
      }
      else {
        new (dst) T(value);
      }
    }
  }

  void copy_in__dynamic(uint index, void *src)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(src != nullptr);

    void *dst = this->element_ptr(index);
    CPPType &type = m_info->type_at_index(index);

    if (m_init[index]) {
      type.copy_to_initialized(src, dst);
    }
    else {
      type.copy_to_uninitialized(src, dst);
      m_init[index] = true;
    }
  }

  template<typename T> void move_in(uint index, T &value)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_info->element_has_type<T>(index));

    T *dst = (T *)this->element_ptr(index);

    if (m_init[index]) {
      *dst = std::move(value);
    }
    else {
      new (dst) T(std::move(value));
      m_init[index] = true;
    }
  }

  void relocate_in__dynamic(uint index, void *src)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(src != nullptr);

    void *dst = this->element_ptr(index);
    CPPType &type = m_info->type_at_index(index);

    if (m_init[index]) {
      type.relocate_to_initialized(src, dst);
    }
    else {
      type.relocate_to_uninitialized(src, dst);
      m_init[index] = true;
    }
  }

  template<typename T> void set(uint index, const T &value)
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value, "can only be used with trivial types");
    this->copy_in<T>(index, value);
  }

  template<typename T> T copy_out(uint index) const
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_info->element_has_type<T>(index));
    BLI_assert(m_init[index]);

    const T *src = (const T *)this->element_ptr(index);
    return *src;
  }

  template<typename T> T relocate_out(uint index)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_info->element_has_type<T>(index));
    BLI_assert(m_init[index]);

    T *stored_value_ptr = (T *)this->element_ptr(index);
    T tmp = std::move(*stored_value_ptr);
    stored_value_ptr->~T();
    m_init[index] = false;

    return tmp;
  }

  void relocate_to_initialized__dynamic(uint index, void *dst)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_init[index]);
    BLI_assert(dst != nullptr);

    void *src = this->element_ptr(index);
    CPPType &type = m_info->type_at_index(index);

    type.relocate_to_initialized(src, dst);
    m_init[index] = false;
  }

  void relocate_to_uninitialized__dynamic(uint index, void *dst)
  {
    BLI_assert(index < m_info->size());
    BLI_assert(m_init[index]);
    BLI_assert(dst != nullptr);

    void *src = this->element_ptr(index);
    CPPType &type = m_info->type_at_index(index);

    type.relocate_to_uninitialized(src, dst);
    m_init[index] = false;
  }

  template<typename T> T get(uint index) const
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value, "can only be used with trivial types");
    return this->copy_out<T>(index);
  }

  template<typename T> T get_cpp_type(uint index) const
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value, "can only be used with trivial types");
    return this->copy_out<T>(index);
  }

  static void CopyElement(const TupleRef &from, uint from_index, TupleRef &to, uint to_index)
  {
    BLI_assert(from.m_init[from_index]);
    BLI_assert(&from.m_info->type_at_index(from_index) == &to.m_info->type_at_index(to_index));

    void *src = from.element_ptr(from_index);
    void *dst = to.element_ptr(to_index);
    CPPType &type = from.m_info->type_at_index(from_index);

    if (to.m_init[to_index]) {
      type.copy_to_initialized(src, dst);
    }
    else {
      type.copy_to_uninitialized(src, dst);
      to.m_init[to_index] = true;
    }
  }

  static void RelocateElement(TupleRef &from, uint from_index, TupleRef &to, uint to_index)
  {
    BLI_assert(from.m_init[from_index]);
    BLI_assert(&from.m_info->type_at_index(from_index) == &to.m_info->type_at_index(to_index));

    void *src = from.element_ptr(from_index);
    void *dst = to.element_ptr(to_index);
    CPPType &type = from.m_info->type_at_index(from_index);

    if (to.m_init[to_index]) {
      type.relocate_to_initialized(src, dst);
    }
    else {
      type.relocate_to_uninitialized(src, dst);
      to.m_init[to_index] = true;
    }
    from.m_init[from_index] = false;
  }

  bool all_initialized() const
  {
    for (uint i = 0; i < m_info->size(); i++) {
      if (!m_init[i]) {
        return false;
      }
    }
    return true;
  }

  void set_all_initialized()
  {
    for (uint i = 0; i < m_info->size(); i++) {
      m_init[i] = true;
    }
  }

  bool all_uninitialized() const
  {
    for (uint i = 0; i < m_info->size(); i++) {
      if (m_init[i]) {
        return false;
      }
    }
    return true;
  }

  void set_all_uninitialized()
  {
    for (uint i = 0; i < m_info->size(); i++) {
      m_init[i] = false;
    }
  }

  void destruct_all()
  {
    if (!m_info->all_trivially_destructible()) {
      uint size = m_info->size();
      for (uint i = 0; i < size; i++) {
        if (m_init[i]) {
          m_info->type_at_index(i).destruct(this->element_ptr(i));
        }
      }
    }
    this->set_all_uninitialized();
  }

  uint size() const
  {
    return m_info->size();
  }

  TupleInfo &info()
  {
    return *m_info;
  }

  void *element_ptr(uint index) const
  {
    uint offset = m_info->offset_of_index(index);
    void *ptr = POINTER_OFFSET(m_data, offset);

    BLI_assert(m_info->type_at_index(index).pointer_has_valid_alignment(ptr));
    return ptr;
  }
};

class DestructingTuple : BLI::NonCopyable, BLI::NonMovable {
 private:
  TupleRef m_tuple_ref;

 public:
  DestructingTuple(TupleInfo &info, void *alignable_buffer)
      : m_tuple_ref(TupleRef::FromAlignableBuffer(info, alignable_buffer))
  {
  }

  ~DestructingTuple()
  {
    m_tuple_ref.destruct_all();
  }

  operator TupleRef &()
  {
    return m_tuple_ref;
  }

  TupleRef *operator->()
  {
    return &m_tuple_ref;
  }
};

}  // namespace BKE

#define BKE_TUPLE_STACK_ALLOC(NAME, INFO_EXPR) \
  BKE::TupleInfo &NAME##_info = (INFO_EXPR); \
  void *NAME##_buffer = alloca(NAME##_info.size_of_alignable_data_and_init()); \
  BKE::DestructingTuple NAME(NAME##_info, NAME##_buffer)

#endif /* __BKE_TUPLE_H__ */
