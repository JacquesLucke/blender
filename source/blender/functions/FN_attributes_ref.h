#ifndef __FN_ATTRIBUTES_REF_H__
#define __FN_ATTRIBUTES_REF_H__

#include "FN_cpp_type.h"
#include "FN_generic_array_ref.h"

#include "BLI_array_cxx.h"
#include "BLI_vector.h"
#include "BLI_vector_set.h"
#include "BLI_string_map.h"
#include "BLI_optional.h"
#include "BLI_monotonic_allocator.h"

namespace FN {

using BLI::Array;
using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::MonotonicAllocator;
using BLI::MutableArrayRef;
using BLI::Optional;
using BLI::StringMap;
using BLI::Vector;
using BLI::VectorSet;

class AttributesInfo;

class AttributesInfoBuilder : BLI::NonCopyable, BLI::NonMovable {
 private:
  MonotonicAllocator<32> m_allocator;
  VectorSet<std::string> m_names;
  Vector<const CPPType *> m_types;
  Vector<void *> m_defaults;

 public:
  AttributesInfoBuilder() = default;
  ~AttributesInfoBuilder();

  template<typename T> void add(StringRef name, const T &default_value)
  {
    this->add(name, CPP_TYPE<T>(), (const void *)&default_value);
  }

  void add(StringRef name, const CPPType &type, const void *default_value = nullptr)
  {
    if (m_names.add(name)) {
      m_types.append(&type);
      void *dst = m_allocator.allocate(type.size(), type.alignment());
      if (default_value == nullptr) {
        type.construct_default(dst);
      }
      else {
        type.copy_to_uninitialized(default_value, dst);
      }
      m_defaults.append(dst);
    }
    else {
      BLI_assert(m_types[m_names.index(name)] == &type);
    }
  }

  bool name_and_type_collide_with_existing(StringRef name, const CPPType &type) const
  {
    int index = m_names.index_try(name);
    if (index == -1) {
      return false;
    }

    const CPPType *existing_type = m_types[index];
    if (*existing_type == type) {
      return false;
    }

    return true;
  }

  uint size() const
  {
    return m_names.size();
  }

  ArrayRef<std::string> names() const
  {
    return m_names;
  }

  ArrayRef<const CPPType *> types() const
  {
    return m_types;
  }

  ArrayRef<const void *> defaults() const
  {
    return ArrayRef<const void *>(m_defaults.begin(), m_defaults.size());
  }

  void add(const AttributesInfoBuilder &other);
  void add(const AttributesInfo &other);
};

class AttributesInfo : BLI::NonCopyable, BLI::NonMovable {
 private:
  MonotonicAllocator<32> m_allocator;
  StringMap<int> m_index_by_name;
  Vector<std::string> m_name_by_index;
  Vector<const CPPType *> m_type_by_index;
  Vector<void *> m_defaults;

 public:
  AttributesInfo() = default;
  AttributesInfo(const AttributesInfoBuilder &builder);
  ~AttributesInfo();

  uint size() const
  {
    return m_name_by_index.size();
  }

  StringRefNull name_of(uint index) const
  {
    return m_name_by_index[index];
  }

  uint index_of(StringRef name) const
  {
    return m_index_by_name.lookup(name);
  }

  const void *default_of(uint index) const
  {
    return m_defaults[index];
  }

  const void *default_of(StringRef name) const
  {
    return this->default_of(this->index_of(name));
  }

  bool has_attribute(StringRef name, const CPPType &type) const
  {
    return this->try_index_of(name, type) >= 0;
  }

  int try_index_of(StringRef name, const CPPType &type) const
  {
    int index = this->try_index_of(name);
    if (index == -1) {
      return -1;
    }
    else if (this->type_of((uint)index) == type) {
      return index;
    }
    else {
      return -1;
    }
  }

  template<typename T> int try_index_of(StringRef name) const
  {
    return this->try_index_of(name, CPP_TYPE<T>());
  }

  int try_index_of(StringRef name) const
  {
    return m_index_by_name.lookup_default(name, -1);
  }

  const CPPType &type_of(uint index) const
  {
    return *m_type_by_index[index];
  }

  const CPPType &type_of(StringRef name) const
  {
    return this->type_of(this->index_of(name));
  }

  ArrayRef<const CPPType *> types() const
  {
    return m_type_by_index;
  }

  IndexRange indices() const
  {
    return IndexRange(this->size());
  }
};

class AttributesRef {
 private:
  const AttributesInfo *m_info;
  ArrayRef<void *> m_buffers;
  IndexRange m_range;

 public:
  AttributesRef(const AttributesInfo &info, ArrayRef<void *> buffers, uint size)
      : AttributesRef(info, buffers, IndexRange(size))
  {
  }

  AttributesRef(const AttributesInfo &info, ArrayRef<void *> buffers, IndexRange range)
      : m_info(&info), m_buffers(buffers), m_range(range)
  {
  }

  uint size() const
  {
    return m_range.size();
  }

  const AttributesInfo &info() const
  {
    return *m_info;
  }

  GenericMutableArrayRef get(uint index) const
  {
    const CPPType &type = m_info->type_of(index);
    void *ptr = POINTER_OFFSET(m_buffers[index], type.size() * m_range.start());
    return GenericMutableArrayRef(m_info->type_of(index), ptr, m_range.size());
  }

  GenericMutableArrayRef get(StringRef name) const
  {
    return this->get(m_info->index_of(name));
  }

  template<typename T> MutableArrayRef<T> get(uint index) const
  {
    BLI_assert(m_info->type_of(index) == CPP_TYPE<T>());
    return MutableArrayRef<T>((T *)m_buffers[index] + m_range.start(), m_range.size());
  }

  template<typename T> MutableArrayRef<T> get(StringRef name) const
  {
    return this->get<T>(m_info->index_of(name));
  }

  Optional<GenericMutableArrayRef> try_get(StringRef name, const CPPType &type) const
  {
    int index = m_info->try_index_of(name, type);
    if (index == -1) {
      return {};
    }
    else {
      return this->get((uint)index);
    }
  }

  template<typename T> Optional<MutableArrayRef<T>> try_get(StringRef name)
  {
    int index = m_info->try_index_of<T>(name);
    if (index == -1) {
      return {};
    }
    else {
      return this->get<T>((uint)index);
    }
  }

  AttributesRef slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  AttributesRef slice(uint start, uint size) const
  {
    return AttributesRef(*m_info, m_buffers, m_range.slice(start, size));
  }

  AttributesRef take_front(uint n) const
  {
    return this->slice(0, n);
  }

  void destruct_and_reorder(IndexMask indices_to_destruct);

  static void RelocateUninitialized(AttributesRef from, AttributesRef to);
};

class AttributesRefGroup {
 private:
  const AttributesInfo *m_info;
  Vector<ArrayRef<void *>> m_buffers;
  Vector<IndexRange> m_ranges;
  uint m_total_size;

 public:
  AttributesRefGroup(const AttributesInfo &info,
                     Vector<ArrayRef<void *>> buffers,
                     Vector<IndexRange> ranges);

  const AttributesInfo &info() const
  {
    return *m_info;
  }

  template<typename T> void set(uint index, ArrayRef<T> data)
  {
    BLI_assert(data.size() == m_total_size);
    BLI_assert(m_info->type_of(index) == CPP_TYPE<T>());

    uint offset = 0;
    for (AttributesRef attributes : *this) {
      MutableArrayRef<T> array = attributes.get<T>(index);
      array.copy_from(data.slice(offset, array.size()));
      offset += array.size();
    }
  }

  template<typename T> void set(StringRef name, ArrayRef<T> data)
  {
    this->set(m_info->index_of(name), data);
  }

  void set(uint index, GenericArrayRef data)
  {
    BLI_assert(data.size() == m_total_size);
    BLI_assert(m_info->type_of(index) == data.type());

    uint offset = 0;
    for (AttributesRef attributes : *this) {
      GenericMutableArrayRef array = attributes.get(index);
      array.type().copy_to_initialized_n(data[offset], array[0], attributes.size());
      offset += attributes.size();
    }
  }

  void set(StringRef name, GenericArrayRef data)
  {
    this->set(m_info->index_of(name), data);
  }

  template<typename T> void set_repeated(uint index, ArrayRef<T> data)
  {
    BLI_assert(m_total_size == 0 || data.size() > 0);
    BLI_assert(m_info->type_of(index) == CPP_TYPE<T>());

    uint src_index = 0;
    for (AttributesRef attributes : *this) {
      MutableArrayRef<T> array = attributes.get<T>(index);

      for (uint i = 0; i < attributes.size(); i++) {
        array[i] = data[src_index];
        src_index++;
        if (src_index == data.size()) {
          src_index = 0;
        }
      }
    }
  }

  template<typename T> void set_repeated(StringRef name, ArrayRef<T> data)
  {
    this->set_repeated(m_info->index_of(name), data);
  }

  void set_repeated(uint index, GenericArrayRef data)
  {
    BLI_assert(m_total_size == 0 || data.size() > 0);
    BLI_assert(m_info->type_of(index) == data.type());

    uint src_index = 0;
    for (AttributesRef attributes : *this) {
      GenericMutableArrayRef array = attributes.get(index);

      for (uint i = 0; i < attributes.size(); i++) {
        array.copy_in__initialized(i, data[src_index]);
        src_index++;
        if (src_index == data.size()) {
          src_index = 0;
        }
      }
    }
  }

  void set_repeated(StringRef name, GenericArrayRef data)
  {
    this->set_repeated(m_info->index_of(name), data);
  }

  template<typename T> void fill(uint index, const T &value)
  {
    BLI_assert(m_info->type_of(index) == CPP_TYPE<T>());

    for (AttributesRef attributes : *this) {
      MutableArrayRef<T> array = attributes.get<T>(index);
      array.fill(value);
    }
  }

  template<typename T> void fill(StringRef name, const T &value)
  {
    this->fill(m_info->index_of(name), value);
  }

  void fill(uint index, const CPPType &type, const void *value)
  {
    BLI_assert(m_info->type_of(index) == type);
    UNUSED_VARS_NDEBUG(type);

    for (AttributesRef attributes : *this) {
      GenericMutableArrayRef array = attributes.get(index);
      array.fill__initialized(value);
    }
  }

  void fill(StringRef name, const CPPType &type, const void *value)
  {
    this->fill(m_info->index_of(name), type, value);
  }

  uint total_size() const
  {
    return m_total_size;
  }

  class Iterator {
   private:
    AttributesRefGroup *m_group;
    uint m_current;

   public:
    Iterator(AttributesRefGroup &group, uint current) : m_group(&group), m_current(current)
    {
    }

    Iterator &operator++()
    {
      m_current++;
      return *this;
    }

    AttributesRef operator*()
    {
      return AttributesRef(
          *m_group->m_info, m_group->m_buffers[m_current], m_group->m_ranges[m_current]);
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_group == b.m_group);
      return a.m_current != b.m_current;
    }
  };

  Iterator begin()
  {
    return Iterator(*this, 0);
  }

  Iterator end()
  {
    return Iterator(*this, m_buffers.size());
  }
};

class AttributesInfoDiff {
 private:
  const AttributesInfo *m_old_info;
  const AttributesInfo *m_new_info;
  Array<int> m_old_to_new_mapping;
  Array<int> m_new_to_old_mapping;

 public:
  AttributesInfoDiff(const AttributesInfo &old_info, const AttributesInfo &new_info);

  void update(uint capacity,
              uint used_size,
              ArrayRef<void *> old_buffers,
              MutableArrayRef<void *> new_buffers) const;

  uint new_buffer_amount() const
  {
    return m_new_info->size();
  }
};

}  // namespace FN

#endif /* __FN_ATTRIBUTES_REF_H__ */
