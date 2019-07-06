#pragma once

/**
 * A tuple is an array that can hold values of different types. It is the primary way to store
 * values of C++ types that you don't know the exact type from.
 *
 * Every tuple links to a TupleMeta instance which contains meta-information about the tuple. Among
 * others it knows which types are stored in the tuple and at which offsets. Furthermore, it owns
 * references to the types. The assumption here is that tuples are much more often created than
 * meta objects. Doing reference counting every time a tuple is created would result in lot of
 * synchronization overhead.
 *
 * Currently, tuples only have normal pointers to their meta objects. So it can be invalidated when
 * it outlives the meta object. In the future it might be necessary to allow tuples to optionally
 * own the tuple meta object, so that it cannot be removed as long as it exists.
 *
 * Tuples can be allocated entirely on the stack to avoid heap allocations. However, due to their
 * dynamic nature, the required memory can differ. There is a macro to simplify the process of
 * allocating a tuple on the stack.
 *
 * A tuple can own the array containing the objects or not, depending on the use case.
 *
 * Every element in the tuple is either initialized or uninitialized. It is explicitely tracked
 * what is the case.
 *
 * The accessors to the tuple fall into two categories:
 *   - Dynamic: When the caller does not know statically which types the tuple contains, it has to
 *       use generic methods. This is less efficient since there might be multiple virtual function
 *       calls.
 *   - Static: Sometimes, the caller knows exactly, which types are at every index in the tuple. In
 *       that case, this information can be used to increase performance and to get a nicer API.
 */

#include "cpp_types.hpp"

namespace FN {

class TupleMeta : public RefCountedBase {
 private:
  SmallVector<SharedType> m_types;
  SmallVector<CPPTypeInfo *> m_type_info;
  SmallVector<uint> m_offsets;
  uint m_size__data;
  uint m_size__data_and_init;
  bool m_all_trivially_destructible;

 public:
  TupleMeta(ArrayRef<SharedType> types = {});

  /**
   * Get an array containing the types of tuples using the meta object.
   */
  const ArrayRef<SharedType> types() const
  {
    return m_types;
  }

  /**
   * Get an array containing the CPPTypeInfo instances of all types.
   */
  const ArrayRef<CPPTypeInfo *> type_infos() const
  {
    return m_type_info;
  }

  /**
   * Get an array containing the byte offsets of every element in the array.
   */
  const ArrayRef<uint> offsets() const
  {
    return m_offsets;
  }

  /**
   * Get the required byte size to store all values in the tuple.
   */
  uint size_of_data() const
  {
    return m_size__data;
  }

  /**
   * Get the size of the boolean buffer that tracks which elements are initialized.
   */
  uint size_of_init() const
  {
    return m_size__data_and_init - m_size__data;
  }

  /**
   * Get the size of the data and initialize buffers combined.
   */
  uint size_of_data_and_init() const
  {
    return m_size__data_and_init;
  }

  /**
   * Get the buffer size that is required to construct the entire tuple in.
   */
  inline uint size_of_full_tuple() const;

  uint element_amount() const
  {
    return m_types.size();
  }

  /**
   * Get the byte size of a specific element.
   */
  uint element_size(uint index) const
  {
    return m_offsets[index + 1] - m_offsets[index];
  }

  /**
   * Return when all types are trivially destructible. Otherwise false.
   * When all types are destructible, no deallocation loop has to run.
   */
  bool all_trivially_destructible() const
  {
    return m_all_trivially_destructible;
  }
};

using SharedTupleMeta = AutoRefCount<TupleMeta>;

class Tuple {
 public:
  Tuple(TupleMeta &meta) : m_meta(&meta)
  {
    m_initialized = (bool *)MEM_calloc_arrayN(m_meta->element_amount(), sizeof(bool), __func__);
    m_data = MEM_mallocN(m_meta->size_of_data(), __func__);
    m_owns_mem = true;
    m_run_destructors = true;
  }

  Tuple(TupleMeta &meta,
        void *data,
        bool *initialized,
        bool was_initialized = false,
        bool run_destructors = true)
      : m_meta(&meta)
  {
    BLI_assert(data != nullptr);
    BLI_assert(initialized != nullptr);
    m_data = data;
    m_initialized = initialized;
    m_owns_mem = false;
    m_run_destructors = run_destructors;
    if (!was_initialized) {
      this->set_all_uninitialized();
    }
  }

  Tuple(TupleMeta &meta, void *buffer) : Tuple(meta, buffer, (bool *)buffer + meta.size_of_data())
  {
  }

  /**
   * Build a new tuple in the prepared buffer. The memory in the buffer is expected to be
   * uninitialized. Furthermore, the buffer must be large enough to hold the entire tuple.
   */
  static Tuple &ConstructInBuffer(TupleMeta &meta, void *buffer)
  {
    Tuple *tuple = new (buffer) Tuple(meta, (char *)buffer + sizeof(Tuple));
    return *tuple;
  }

  /* Has to be implemented explicitely in the future. */
  Tuple(const Tuple &tuple) = delete;

  ~Tuple()
  {
    if (m_run_destructors) {
      this->destruct_all();
    }
    if (m_owns_mem) {
      MEM_freeN(m_data);
      MEM_freeN(m_initialized);
    }
  }

  /**
   * Copy a value of type T to the given index. The caller is expected to know that T actually
   * belongs to this type.
   */
  template<typename T> inline void copy_in(uint index, const T &value)
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(sizeof(T) == m_meta->element_size(index));

    T *dst = (T *)this->element_ptr(index);
    if (std::is_trivial<T>::value) {
      std::memcpy(dst, &value, sizeof(T));
    }
    else {

      if (m_initialized[index]) {
        std::copy_n(&value, 1, dst);
      }
      else {
        std::uninitialized_copy_n(&value, 1, dst);
      }
    }

    m_initialized[index] = true;
  }

  /**
   * Copy a value from src to the given index in the tuple.
   */
  inline void copy_in__dynamic(uint index, void *src)
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(src != nullptr);

    void *dst = this->element_ptr(index);
    auto *type_info = m_meta->type_infos()[index];

    if (m_initialized[index]) {
      type_info->copy_to_initialized(src, dst);
    }
    else {
      type_info->copy_to_uninitialized(src, dst);
      m_initialized[index] = true;
    }
  }

  /**
   * Move a value of type T into the tuple. Note, that the destructor on the original object will
   * not be called, because this will usually be done automatically when it goes out of scope.
   * The caller is expected to know that the type T actually belongs to this index.
   */
  template<typename T> inline void move_in(uint index, T &value)
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(sizeof(T) == m_meta->element_size(index));

    T *dst = (T *)this->element_ptr(index);

    if (m_initialized[index]) {
      std::copy_n(std::make_move_iterator(&value), 1, dst);
    }
    else {
      std::uninitialized_copy_n(std::make_move_iterator(&value), 1, dst);
      m_initialized[index] = true;
    }
  }

  /**
   * Copy the value from src into the tuple and destroy the original value at src.
   */
  inline void relocate_in__dynamic(uint index, void *src)
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(src != nullptr);

    void *dst = this->element_ptr(index);
    auto *type_info = m_meta->type_infos()[index];

    if (m_initialized[index]) {
      type_info->relocate_to_initialized(src, dst);
    }
    else {
      type_info->relocate_to_uninitialized(src, dst);
      m_initialized[index] = true;
    }
  }

  /**
   * Copy the value to the given index. This method only works with trivial types.
   */
  template<typename T> inline void set(uint index, const T &value)
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value,
                      "this method can be used with trivial types only");
    this->copy_in<T>(index, value);
  }

  /**
   * Return a copy of the value at the given index. The caller is expected to know that the index
   * actually contains a value of type T.
   * Asserts when the value was not initialized.
   */
  template<typename T> inline T copy_out(uint index) const
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(sizeof(T) == m_meta->element_size(index));
    BLI_assert(m_initialized[index]);

    return *(T *)this->element_ptr(index);
  }

  /**
   * Return the value at the given index and destroy the value in the tuple. Afterwards, this index
   * will contain uininitialized memory. The caller is expected to know that T is the correct type
   * for that index.
   * Asserts when the value was not initialized.
   */
  template<typename T> inline T relocate_out(uint index) const
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(sizeof(T) == m_meta->element_size(index));
    BLI_assert(m_initialized[index]);

    T &value = this->element_ref<T>(index);
    T tmp = std::move(value);
    value.~T();
    m_initialized[index] = false;

    return tmp;
  }

  /**
   * Copy the value from the tuple into the dst buffer.
   * Asserts when the value was not initialized.
   */
  inline void relocate_out__dynamic(uint index, void *dst) const
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(m_initialized[index]);
    BLI_assert(dst != nullptr);

    void *src = this->element_ptr(index);
    auto *type_info = m_meta->type_infos()[index];

    type_info->relocate_to_uninitialized(src, dst);

    m_initialized[index] = false;
  }

  /**
   * Return a copy of the value in the tuple at the given index. This only works with trivial
   * types.
   * Asserts when the value was not initialized.
   */
  template<typename T> inline T get(uint index) const
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value,
                      "this method can be used with trivial types only");
    return this->copy_out<T>(index);
  }

  /**
   * Return a reference to a value in the tuple.
   * Asserts when the value is not initialized.
   */
  template<typename T> inline T &get_ref(uint index) const
  {
    BLI_assert(index < m_meta->element_amount());
    BLI_assert(m_initialized[index]);
    return this->element_ref<T>(index);
  }

  /**
   * Return true when the value at the given index is initialized, otherwise false.
   */
  inline bool is_initialized(uint index) const
  {
    BLI_assert(index < m_meta->element_amount());
    return m_initialized[index];
  }

  /**
   * Copy a value between two different location in different tuples.
   * Asserts when the source value is not initialized.
   */
  static inline void copy_element(const Tuple &from, uint from_index, Tuple &to, uint to_index)
  {
    BLI_assert(from.m_initialized[from_index]);
    BLI_assert(from.m_meta->types()[from_index] == to.m_meta->types()[to_index]);

    void *src = from.element_ptr(from_index);
    void *dst = to.element_ptr(to_index);
    CPPTypeInfo *type_info = from.m_meta->type_infos()[from_index];

    if (to.m_initialized[to_index]) {
      type_info->copy_to_initialized(src, dst);
    }
    else {
      type_info->copy_to_uninitialized(src, dst);
      to.m_initialized[to_index] = true;
    }
  }

  /**
   * Copy a value between two different locations in different tuples and destroy the original
   * value.
   * Asserts when the source value is not initialized.
   */
  static inline void relocate_element(Tuple &from, uint from_index, Tuple &to, uint to_index)
  {
    BLI_assert(from.m_initialized[from_index]);
    BLI_assert(from.m_meta->types()[from_index] == to.m_meta->types()[to_index]);

    void *src = from.element_ptr(from_index);
    void *dst = to.element_ptr(to_index);
    CPPTypeInfo *type_info = from.m_meta->type_infos()[from_index];

    if (to.m_initialized[to_index]) {
      type_info->relocate_to_initialized(src, dst);
    }
    else {
      type_info->relocate_to_uninitialized(src, dst);
      to.m_initialized[to_index] = true;
    }

    from.m_initialized[from_index] = false;
  }

  /**
   * Initialize the value at the given index with a default value.
   */
  inline void init_default(uint index) const
  {
    CPPTypeInfo *type_info = m_meta->type_infos()[index];
    void *ptr = this->element_ptr(index);

    if (m_initialized[index]) {
      type_info->destruct_type(ptr);
    }

    type_info->construct_default(ptr);
    m_initialized[index] = true;
  }

  /**
   * Initialize all values in the tuple with a default value.
   */
  inline void init_default_all() const
  {
    for (uint i = 0; i < m_meta->element_amount(); i++) {
      this->init_default(i);
    }
  }

  /**
   * Get the address of the buffer containing all values.
   */
  void *data_ptr() const
  {
    return m_data;
  }

  /**
   * Get the address of the buffer containing the byte offsets of all values.
   */
  const uint *offsets_ptr() const
  {
    return m_meta->offsets().begin();
  }

  /**
   * Return when all values are initialized, otherwise false.
   */
  bool all_initialized() const
  {
    for (uint i = 0; i < m_meta->element_amount(); i++) {
      if (!m_initialized[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Return true when no value is initialized, otherwise false.
   */
  bool all_uninitialized() const
  {
    for (uint i = 0; i < m_meta->element_amount(); i++) {
      if (m_initialized[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Mark all values as initialized. This should only be done when the buffer has been initialized
   * outside of the tuple methods.
   */
  void set_all_initialized()
  {
    for (uint i = 0; i < m_meta->element_amount(); i++) {
      m_initialized[i] = true;
    }
  }

  /**
   * Mark all values as uninitialized. This should only be done when the values are destroyed
   * outside of the tuple methods.
   */
  void set_all_uninitialized()
  {
    for (uint i = 0; i < m_meta->element_amount(); i++) {
      m_initialized[i] = false;
    }
  }

  /**
   * Destroy all initialized values in the tuple.
   */
  void destruct_all()
  {
    if (m_meta->all_trivially_destructible()) {
      this->set_all_uninitialized();
    }
    else {
      for (uint i = 0; i < m_meta->element_amount(); i++) {
        if (m_initialized[i]) {
          m_meta->type_infos()[i]->destruct_type(this->element_ptr(i));
          m_initialized[i] = false;
        }
      }
    }
  }

  /**
   * Return the number of elements in the tuple.
   */
  uint size() const
  {
    return m_meta->element_amount();
  }

  void print_initialized(std::string name = "");

 private:
  inline void *element_ptr(uint index) const
  {
    return (void *)((char *)m_data + m_meta->offsets()[index]);
  }

  template<typename T> inline T &element_ref(uint index) const
  {
    return *(T *)this->element_ptr(index);
  }

  void *m_data;
  bool *m_initialized;
  bool m_owns_mem;
  bool m_run_destructors;
  TupleMeta *m_meta;
};

inline uint TupleMeta::size_of_full_tuple() const
{
  return sizeof(Tuple) + this->size_of_data_and_init();
}

} /* namespace FN */

/**
 * Allocate a new tuple entirely on the stack with the given meta object.
 */
#define FN_TUPLE_STACK_ALLOC(name, meta_expr) \
  FN::TupleMeta &name##_meta = (meta_expr); \
  void *name##_buffer = alloca(name##_meta.size_of_data_and_init()); \
  FN::Tuple name(name##_meta, name##_buffer);
