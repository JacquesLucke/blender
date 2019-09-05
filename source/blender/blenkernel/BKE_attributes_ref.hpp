/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * This file provides classes that allow referencing multiple attribute arrays at the same time.
 * Every attribute array has an element-type, name and default value.
 */

#pragma once

#include <string>

#include "BLI_array_ref.hpp"
#include "BLI_math.hpp"
#include "BLI_optional.hpp"
#include "BLI_index_range.hpp"
#include "BLI_set_vector.hpp"
#include "BLI_set.hpp"
#include "BLI_string_map.hpp"
#include "BLI_string_ref.hpp"
#include "BLI_vector.hpp"

namespace BKE {

using BLI::ArrayRef;
using BLI::float2;
using BLI::float3;
using BLI::IndexRange;
using BLI::MutableArrayRef;
using BLI::Optional;
using BLI::rgba_b;
using BLI::rgba_f;
using BLI::SetVector;
using BLI::StringMap;
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

/**
 * Possible types of attributes. All types are expected to be POD (plain old data).
 * New types can be added when necessary.
 */
enum AttributeType {
  Byte,
  Integer,
  Float,
  Float2,
  Float3,
  RGBA_b,
  RGBA_f,
};

template<typename T> struct attribute_type_by_type {
};

#define ATTRIBUTE_TYPE_BY_TYPE(CPP_TYPE, ATTRIBUTE_TYPE) \
  template<> struct attribute_type_by_type<CPP_TYPE> { \
    static const AttributeType value = AttributeType::ATTRIBUTE_TYPE; \
  }

ATTRIBUTE_TYPE_BY_TYPE(uint8_t, Byte);
ATTRIBUTE_TYPE_BY_TYPE(int32_t, Integer);
ATTRIBUTE_TYPE_BY_TYPE(float, Float);
ATTRIBUTE_TYPE_BY_TYPE(float2, Float2);
ATTRIBUTE_TYPE_BY_TYPE(float3, Float3);
ATTRIBUTE_TYPE_BY_TYPE(rgba_b, RGBA_b);
ATTRIBUTE_TYPE_BY_TYPE(rgba_f, RGBA_f);

#undef ATTRIBUTE_TYPE_BY_TYPE

/**
 * Get the size of an attribute type.
 */
inline uint size_of_attribute_type(AttributeType type)
{
  switch (type) {
    case AttributeType::Byte:
      return sizeof(uint8_t);
    case AttributeType::Integer:
      return sizeof(int32_t);
    case AttributeType::Float:
      return sizeof(float);
    case AttributeType::Float2:
      return sizeof(float2);
    case AttributeType::Float3:
      return sizeof(float3);
    case AttributeType::RGBA_b:
      return sizeof(rgba_b);
    case AttributeType::RGBA_f:
      return sizeof(rgba_f);
  };
  BLI_assert(false);
  return 0;
}

#define MAX_ATTRIBUTE_SIZE sizeof(rgba_f)

/**
 * Container that is large enough to hold one value of any attribute type.
 */
struct AnyAttributeValue {
  char storage[MAX_ATTRIBUTE_SIZE];

  template<typename T> static AnyAttributeValue FromValue(T value)
  {
    BLI_STATIC_ASSERT(attribute_type_by_type<T>::value >= 0, "");
    BLI_STATIC_ASSERT(sizeof(T) <= MAX_ATTRIBUTE_SIZE, "");
    AnyAttributeValue attribute;
    memcpy(attribute.storage, &value, sizeof(T));
    return attribute;
  }
};

class AttributesInfo;

class AttributesDeclaration {
 private:
  SetVector<std::string> m_names;
  Vector<AttributeType> m_types;
  Vector<AnyAttributeValue> m_defaults;

  friend AttributesInfo;

 public:
  AttributesDeclaration() = default;

  template<typename T> void add(StringRef name, T default_value)
  {
    if (m_names.add(name)) {
      AttributeType type = attribute_type_by_type<T>::value;
      m_types.append(type);
      m_defaults.append(AnyAttributeValue::FromValue(default_value));
    }
  }

  uint size() const
  {
    return m_names.size();
  }

  void join(AttributesDeclaration &other);
  void join(AttributesInfo &other);
};

/**
 * Contains information about a set of attributes. Every attribute is identified by a unique name
 * and a unique index. So two attributes of different types have to have different names.
 *
 * Furthermore, every attribute has a default value.
 */
class AttributesInfo {
 private:
  StringMap<int> m_index_by_name;
  Vector<std::string> m_name_by_index;
  Vector<AttributeType> m_type_by_index;
  Vector<AnyAttributeValue> m_default_by_index;

  friend AttributesDeclaration;

 public:
  AttributesInfo() = default;
  AttributesInfo(AttributesDeclaration &builder);

  /**
   * Get the number of different attributes.
   */
  uint size() const
  {
    return m_name_by_index.size();
  }

  /**
   * Get the attribute name that corresponds to an index.
   * Asserts when the index is too large.
   */
  StringRefNull name_of(uint index) const
  {
    return m_name_by_index[index];
  }

  /**
   * Get the type of an attribute identified by its index.
   * Asserts when the index is too large.
   */
  AttributeType type_of(uint index) const
  {
    return m_type_by_index[index];
  }

  /**
   * Get the type of an attribute identified by its name.
   * Asserts when the name does not exist.
   */
  AttributeType type_of(StringRef name) const
  {
    return this->type_of(this->attribute_index(name));
  }

  /**
   * Get the types of all attributes. The index into the array is the index of the corresponding
   * attribute.
   */
  ArrayRef<AttributeType> types() const
  {
    return m_type_by_index;
  }

  /**
   * Get the index corresponding to an attribute name.
   * Returns -1 when the attribute does not exist.
   */
  int attribute_index_try(StringRef name) const
  {
    return m_index_by_name.lookup_default(name, -1);
  }

  /**
   * Get the index corresponding to an attribute with the given name and type.
   * Returns -1 when the attribute does not exist.
   */
  int attribute_index_try(StringRef name, AttributeType type) const
  {
    int index = this->attribute_index_try(name);
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

  /**
   * Get the index corresponding to an attribute name.
   * Asserts when the attribute does not exist.
   */
  uint attribute_index(StringRef name) const
  {
    int index = m_index_by_name.lookup(name);
    return (uint)index;
  }

  /**
   * Get a range with all attribute indices.
   * The range will start at 0.
   */
  IndexRange attribute_indices() const
  {
    return IndexRange(this->size());
  }

  /**
   * Get a pointer to the default value of an attribute.
   */
  void *default_value_ptr(uint index) const
  {
    BLI_assert(index < this->size());
    return (void *)m_default_by_index[index].storage;
  }

  /**
   * Don't do a deep comparison for now. This might change later.
   */
  friend bool operator==(const AttributesInfo &a, const AttributesInfo &b)
  {
    return &a == &b;
  }
};

/**
 * The main class used to interact with attributes. It only references a set of arrays, so it can
 * be passed by value.
 */
class AttributesRef {
 private:
  AttributesInfo *m_info;
  ArrayRef<void *> m_buffers;
  IndexRange m_range;

 public:
  AttributesRef(AttributesInfo &info, ArrayRef<void *> buffers, uint size)
      : AttributesRef(info, buffers, IndexRange(size))
  {
  }

  AttributesRef(AttributesInfo &info, ArrayRef<void *> buffers, IndexRange range)
      : m_info(&info), m_buffers(buffers), m_range(range)
  {
  }

  /**
   * Get the number of referenced elements.
   */
  uint size() const
  {
    return m_range.size();
  }

  /**
   * Get information about the referenced attributes.
   */
  AttributesInfo &info()
  {
    return *m_info;
  }

  /**
   * Get the index of an attributed identified by a name.
   */
  uint attribute_index(StringRef name)
  {
    return this->info().attribute_index(name);
  }

  /**
   * Get the size of an element in one attribute.
   */
  uint attribute_size(uint index)
  {
    return size_of_attribute_type(this->info().type_of(index));
  }

  /**
   * Get the raw pointer to the buffer that contains attribute values.
   */
  void *get_ptr(uint index) const
  {
    void *ptr = m_buffers[index];
    AttributeType type = m_info->type_of(index);
    uint size = size_of_attribute_type(type);
    return POINTER_OFFSET(ptr, m_range.start() * size);
  }

  /**
   * Initialize an attribute array using its default value.
   */
  void init_default(uint index)
  {
    void *default_value = m_info->default_value_ptr(index);
    void *dst = this->get_ptr(index);
    AttributeType type = m_info->type_of(index);
    uint element_size = size_of_attribute_type(type);

    for (uint i : m_range) {
      memcpy(POINTER_OFFSET(dst, element_size * i), default_value, element_size);
    }
  }

  void init_default(StringRef name)
  {
    this->init_default(this->attribute_index(name));
  }

  /**
   * Get access to the underlying attribute arrays.
   * Asserts when the attribute does not exists.
   */
  template<typename T> MutableArrayRef<T> get(uint index) const
  {
    BLI_assert(attribute_type_by_type<T>::value == m_info->type_of(index));
    void *ptr = this->get_ptr(index);
    return MutableArrayRef<T>((T *)ptr, m_range.size());
  }

  template<typename T> MutableArrayRef<T> get(StringRef name)
  {
    uint index = this->attribute_index(name);
    return this->get<T>(index);
  }

  /**
   * Get access to the arrays.
   * Does not assert when the attribute does not exist.
   */
  template<typename T> Optional<MutableArrayRef<T>> try_get(StringRef name)
  {
    int index = this->info().attribute_index_try(name, attribute_type_by_type<T>::value);
    if (index == -1) {
      return {};
    }
    else {
      return this->get<T>((uint)index);
    }
  }

  /**
   * Get a continuous slice of the attribute arrays.
   */
  AttributesRef slice(uint start, uint size) const
  {
    return AttributesRef(*m_info, m_buffers, m_range.slice(start, size));
  }

  /**
   * Create a new slice containing only the first n elements.
   */
  AttributesRef take_front(uint n) const
  {
    return AttributesRef(*m_info, m_buffers, m_range.slice(0, n));
  }
};

class AttributesRefGroup {
 private:
  AttributesInfo *m_attributes_info;
  Vector<ArrayRef<void *>> m_buffers;
  Vector<IndexRange> m_ranges;
  uint m_size;

 public:
  AttributesRefGroup(AttributesInfo &attributes_info,
                     Vector<ArrayRef<void *>> buffers,
                     Vector<IndexRange> ranges);

  template<typename T> void set(uint index, ArrayRef<T> data)
  {
    BLI_assert(data.size() == m_size);
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->set_elements(index, (void *)data.begin());
  }

  template<typename T> void set(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->set<T>(index, data);
  }

  template<typename T> void set_repeated(uint index, ArrayRef<T> data)
  {
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->set_repeated_elements(
        index, (void *)data.begin(), data.size(), m_attributes_info->default_value_ptr(index));
  }

  template<typename T> void set_repeated(StringRef name, ArrayRef<T> data)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->set_repeated<T>(index, data);
  }

  template<typename T> void fill(uint index, T value)
  {
    BLI_assert(m_attributes_info->type_of(index) == attribute_type_by_type<T>::value);
    this->fill_elements(index, (void *)&value);
  }

  template<typename T> void fill(StringRef name, T value)
  {
    uint index = m_attributes_info->attribute_index(name);
    this->fill<T>(index, value);
  }

  AttributesInfo &attributes_info()
  {
    return *m_attributes_info;
  }

  class Iterator {
   private:
    AttributesRefGroup *m_group;
    uint m_current;

   public:
    Iterator(AttributesRefGroup *group, uint current) : m_group(group), m_current(current)
    {
    }

    Iterator &operator++()
    {
      m_current++;
      return *this;
    }

    AttributesRef operator*()
    {
      return AttributesRef(*m_group->m_attributes_info,
                           m_group->m_buffers[m_current],
                           m_group->m_ranges[m_current]);
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_group == b.m_group);
      return a.m_current != b.m_current;
    }
  };

  Iterator begin()
  {
    return Iterator(this, 0);
  }

  Iterator end()
  {
    return Iterator(this, m_buffers.size());
  }

 private:
  void set_elements(uint index, void *data);
  void set_repeated_elements(uint index,
                             void *data,
                             uint data_element_amount,
                             void *default_value);
  void fill_elements(uint index, void *value);
};

}  // namespace BKE
