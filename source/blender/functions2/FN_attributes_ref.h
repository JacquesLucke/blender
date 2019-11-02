#ifndef __FN_ATTRIBUTES_REF_H__
#define __FN_ATTRIBUTES_REF_H__

#include "FN_cpp_type.h"

#include "BLI_vector.h"
#include "BLI_vector_set.h"
#include "BLI_string_map.h"
#include "BLI_optional.h"

namespace FN {

using BLI::ArrayRef;
using BLI::IndexRange;
using BLI::MutableArrayRef;
using BLI::Optional;
using BLI::StringMap;
using BLI::Vector;
using BLI::VectorSet;

class AttributesInfo;

class AttributesInfoBuilder {
 private:
  VectorSet<std::string> m_names;
  Vector<const CPPType *> m_types;

 public:
  AttributesInfoBuilder() = default;

  template<typename T> void add(StringRef name)
  {
    this->add(name, GET_TYPE<T>());
  }

  void add(StringRef name, const CPPType &type)
  {
    if (m_names.add(name)) {
      m_types.append(&type);
    }
    else {
      BLI_assert(m_types[m_names.index(name)] == &type);
    }
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

  void add(const AttributesInfoBuilder &other);
  void add(const AttributesInfo &other);
};

class AttributesInfo {
 private:
  StringMap<int> m_index_by_name;
  Vector<std::string> m_name_by_index;
  Vector<const CPPType *> m_type_by_index;

 public:
  AttributesInfo() = default;
  AttributesInfo(const AttributesInfoBuilder &builder);

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

  int index_of_try(StringRef name, const CPPType &type) const
  {
    int index = this->index_of_try(name);
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

  template<typename T> int index_of_try(StringRef name) const
  {
    return this->index_of_try(name, GET_TYPE<T>());
  }

  int index_of_try(StringRef name) const
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

  const AttributesInfo &info()
  {
    return *m_info;
  }

  void *get(uint index) const
  {
    void *ptr = m_buffers[index];
    uint size = m_info->type_of(index).size();
    return POINTER_OFFSET(ptr, m_range.start() * size);
  }

  template<typename T> MutableArrayRef<T> get(uint index) const
  {
    BLI_assert(m_info->type_of(index) == GET_TYPE<T>());
    return MutableArrayRef<T>((T *)m_buffers[index] + m_range.start(), m_range.size());
  }

  template<typename T> MutableArrayRef<T> get(StringRef name) const
  {
    return this->get<T>(m_info->index_of(name));
  }

  template<typename T> Optional<MutableArrayRef<T>> try_get(StringRef name)
  {
    int index = m_info->index_of_try<T>(name);
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

  template<typename T> void set(uint index, ArrayRef<T> data)
  {
    BLI_assert(data.size() == m_total_size);
    BLI_assert(m_info->type_of(index) == GET_TYPE<T>());

    uint offset = 0;
    for (AttributesRef attributes : *this) {
      MutableArrayRef<T> array = attributes.get<T>(index);
      array.copy_from(data.slice(offset, array.size()));
      offset += array.size();
    }
  }

  const AttributesInfo &info() const
  {
    return *m_info;
  }

  template<typename T> void set(StringRef name, ArrayRef<T> data)
  {
    this->set(m_info->index_of(name), data);
  }

  template<typename T> void set_repeated(uint index, ArrayRef<T> data)
  {
    BLI_assert(m_total_size == 0 || data.size() > 0);
    BLI_assert(m_info->type_of(index) == GET_TYPE<T>());

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

  template<typename T> void fill(uint index, const T &value)
  {
    BLI_assert(m_info->type_of(index) == GET_TYPE<T>());

    for (AttributesRef attributes : *this) {
      MutableArrayRef<T> array = attributes.get<T>(index);
      array.fill(value);
    }
  }

  template<typename T> void fill(StringRef name, const T &value)
  {
    this->fill(m_info->index_of(name), value);
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

}  // namespace FN

#endif /* __FN_ATTRIBUTES_REF_H__ */
