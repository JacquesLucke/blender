#pragma once

#include <string>

#include "BLI_array_ref.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_math.hpp"
#include "BLI_string_ref.hpp"
#include "BLI_range.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_fixed_array_allocator.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::FixedArrayAllocator;
using BLI::float3;
using BLI::Range;
using BLI::SmallSetVector;
using BLI::SmallVector;
using BLI::StringRef;
using BLI::StringRefNull;

/**
 * Possible types of attributes. All types are expected to be POD (plain old data).
 * New types can be added when necessary.
 */
enum AttributeType {
  Byte,
  Float,
  Float3,
};

/**
 * Get the size of an attribute type.
 *
 * TODO(jacques): Figure out how to make type.size() work nicely instead.
 */
inline uint size_of_attribute_type(AttributeType type)
{
  switch (type) {
    case AttributeType::Byte:
      return sizeof(uint8_t);
    case AttributeType::Float:
      return sizeof(float);
    case AttributeType::Float3:
      return sizeof(float3);
    default:
      BLI_assert(false);
      return 0;
  };
}

/**
 * Contains information about a set of attributes. Every attribute is identified by a unique name
 * and a unique index. So two attributes of different types have to have different names.
 *
 * The attributes are sorted such that attributes with the same type have consecutive indices.
 *
 * Furthermore, every attribute has a default value.
 */
class AttributesInfo {
 private:
  Range<uint> m_byte_attributes;
  Range<uint> m_float_attributes;
  Range<uint> m_float3_attributes;
  SmallVector<AttributeType> m_types;
  SmallSetVector<std::string> m_indices;

  SmallVector<uint8_t> m_byte_defaults;
  SmallVector<float> m_float_defaults;
  SmallVector<float3> m_float3_defaults;

 public:
  AttributesInfo() = default;
  AttributesInfo(ArrayRef<std::string> byte_names,
                 ArrayRef<std::string> float_names,
                 ArrayRef<std::string> float3_names);

  /**
   * Get the number of different attributes.
   */
  uint amount() const
  {
    return m_indices.size();
  }

  /**
   * Get the attribute name that corresponds to an index.
   * Asserts when the index is too large.
   */
  StringRefNull name_of(uint index) const
  {
    return m_indices[index];
  }

  /**
   * Get the type of an attribute identified by its index.
   * Asserts when the index is too large.
   */
  AttributeType type_of(uint index) const
  {
    return m_types[index];
  }

  /**
   * Get the types of all attributes. The index into the array is the index of the corresponding
   * attribute.
   */
  ArrayRef<AttributeType> types() const
  {
    return m_types;
  }

  /**
   * Get the index corresponding to an attribute name.
   * Returns -1 when the attribute does not exist.
   */
  int attribute_index_try(StringRef name) const
  {
    return m_indices.index(name.to_std_string());
  }

  /**
   * Get the index corresponding to an attribute name.
   * Asserts when the attribute does not exist.
   */
  uint attribute_index(StringRef name) const
  {
    int index = this->attribute_index_try(name);
    BLI_assert(index >= 0);
    return (uint)index;
  }

  /**
   * Get a range with all attribute indices.
   * The range will start at 0.
   */
  Range<uint> attribute_indices() const
  {
    return Range<uint>(0, m_indices.size());
  }

  /**
   * Get a range with all byte attribute indices.
   */
  Range<uint> byte_attributes() const
  {
    return m_byte_attributes;
  }

  /**
   * Get a range with all float attribute indices.
   */
  Range<uint> float_attributes() const
  {
    return m_float_attributes;
  }

  /**
   * Get a range with all float3 attribute indices.
   */
  Range<uint> float3_attributes() const
  {
    return m_float3_attributes;
  }

  /**
   * Get a pointer to the default value of an attribute.
   */
  void *default_value_ptr(uint index) const
  {
    BLI_assert(index < m_indices.size());
    AttributeType type = this->type_of(index);
    switch (type) {
      case AttributeType::Byte:
        return (void *)&m_byte_defaults[index - m_byte_attributes.first()];
      case AttributeType::Float:
        return (void *)&m_float_defaults[index - m_float_attributes.first()];
      case AttributeType::Float3:
        return (void *)&m_float3_defaults[index - m_float3_attributes.first()];
    }
    BLI_assert(false);
    return nullptr;
  }

  /**
   * Don't do a deep comparison for now. This might change later.
   */
  friend bool operator==(const AttributesInfo &a, const AttributesInfo &b)
  {
    return &a == &b;
  }
};

class AttributeArrays;

/**
 * Contains a memory buffer for every attribute in an AttributesInfo object.
 * All buffers have equal element-length but not necessarily equal byte-length.
 *
 * The pointers are not owned by this structure. They are passed on creation and have to be freed
 * manually. This is necessary because in different contexts, it makes sense to allocate the
 * buffers in different ways. Nevertheless, there are some utilities to simplify allocation and
 * deallocation in common cases.
 *
 * Most code does not use this class directly. Instead it uses AttributeArrays, which is just a
 * slice of this.
 */
class AttributeArraysCore {
 private:
  AttributesInfo *m_info;
  SmallVector<void *> m_arrays;
  uint m_size = 0;

 public:
  AttributeArraysCore(AttributesInfo &info, ArrayRef<void *> arrays, uint size);
  ~AttributeArraysCore();

  /**
   * Create a new instance in which the pointers are all separately allocated using MEM_mallocN.
   */
  static AttributeArraysCore NewWithSeparateAllocations(AttributesInfo &info, uint size);
  /**
   * Free all buffers separately using MEM_freeN.
   */
  void free_buffers();

  /**
   * Create a new instance in which all pointers are separately allocated from a
   * fixed-array-allocator. No separate length has to be provided, since the allocator only
   * allocates arrays of one specific length.
   */
  static AttributeArraysCore NewWithArrayAllocator(AttributesInfo &info,
                                                   FixedArrayAllocator &allocator);

  /**
   * Deallocate pointers in the given fixed-array-allocator.
   */
  void deallocate_in_array_allocator(FixedArrayAllocator &allocator);

  /**
   * Get information about the stored attributes.
   */
  AttributesInfo &info();

  /**
   * Get the raw pointer to the beginning of an attribute array identified by an index.
   */
  void *get_ptr(uint index);

  /**
   * Get the type of an attribute identified by an index.
   */
  AttributeType get_type(uint index);

  /**
   * Get a slice containing everything for further processing.
   */
  AttributeArrays slice_all();

  /**
   * Get the number of elements stored per attribute.
   */
  uint size() const;

  /**
   * Get all raw pointers.
   */
  ArrayRef<void *> pointers();
};

/**
 * The main class used to interact with attributes. It represents a continuous slice of an
 * AttributeArraysCore instance. So, it is very light weight and can be passed by value.
 */
class AttributeArrays {
 private:
  AttributeArraysCore &m_core;
  uint m_start, m_size;

 public:
  AttributeArrays(AttributeArraysCore &core, uint start, uint size);

  /**
   * Get the number of referenced elements.
   */
  uint size() const;

  /**
   * Get information about the referenced attributes.
   */
  AttributesInfo &info();

  /**
   * Get the index of an attributed identified by a name.
   */
  uint attribute_index(StringRef name);

  /**
   * Get the size of an element in one attribute.
   */
  uint attribute_stride(uint index);

  /**
   * Get the raw pointer to the buffer that contains attribute values.
   */
  void *get_ptr(uint index) const;

  /**
   * Initialize an attribute array using its default value.
   */
  void init_default(uint index);
  void init_default(StringRef name);

  /**
   * Get access do the underlying attribute arrays.
   */
  ArrayRef<uint8_t> get_byte(uint index) const;
  ArrayRef<uint8_t> get_byte(StringRef name);
  ArrayRef<float> get_float(uint index) const;
  ArrayRef<float> get_float(StringRef name);
  ArrayRef<float3> get_float3(uint index) const;
  ArrayRef<float3> get_float3(StringRef name);

  /**
   * Get a continuous slice of the attribute arrays.
   */
  AttributeArrays slice(uint start, uint size) const;

  /**
   * Create a new slice containing only the first n elements.
   */
  AttributeArrays take_front(uint n) const;
};

/* Attribute Arrays Core
 *****************************************/

inline AttributesInfo &AttributeArraysCore::info()
{
  return *m_info;
}

inline void *AttributeArraysCore::get_ptr(uint index)
{
  return m_arrays[index];
}

inline AttributeType AttributeArraysCore::get_type(uint index)
{
  return m_info->type_of(index);
}

inline AttributeArrays AttributeArraysCore::slice_all()
{
  return AttributeArrays(*this, 0, m_size);
}

inline uint AttributeArraysCore::size() const
{
  return m_size;
}

inline ArrayRef<void *> AttributeArraysCore::pointers()
{
  return m_arrays;
}

/* Attribute Arrays
 ******************************************/

inline AttributeArrays::AttributeArrays(AttributeArraysCore &core, uint start, uint size)
    : m_core(core), m_start(start), m_size(size)
{
  BLI_assert(m_start + m_size <= m_core.size());
}

inline uint AttributeArrays::size() const
{
  return m_size;
}

inline AttributesInfo &AttributeArrays::info()
{
  return m_core.info();
}

inline uint AttributeArrays::attribute_index(StringRef name)
{
  return this->info().attribute_index(name);
}

inline uint AttributeArrays::attribute_stride(uint index)
{
  return size_of_attribute_type(this->info().type_of(index));
}

inline void *AttributeArrays::get_ptr(uint index) const
{
  void *ptr = m_core.get_ptr(index);
  AttributeType type = m_core.get_type(index);
  uint size = size_of_attribute_type(type);
  return POINTER_OFFSET(ptr, m_start * size);
}

inline void AttributeArrays::init_default(uint index)
{
  void *default_value = m_core.info().default_value_ptr(index);
  void *dst = this->get_ptr(index);
  AttributeType type = m_core.get_type(index);
  uint element_size = size_of_attribute_type(type);

  for (uint i = 0; i < m_size; i++) {
    memcpy(POINTER_OFFSET(dst, element_size * i), default_value, element_size);
  }
}

inline void AttributeArrays::init_default(StringRef name)
{
  this->init_default(this->attribute_index(name));
}

inline ArrayRef<uint8_t> AttributeArrays::get_byte(uint index) const
{
  BLI_assert(m_core.get_type(index) == AttributeType::Byte);
  return ArrayRef<uint8_t>((uint8_t *)m_core.get_ptr(index) + m_start, m_size);
}

inline ArrayRef<uint8_t> AttributeArrays::get_byte(StringRef name)
{
  return this->get_byte(this->attribute_index(name));
}

inline ArrayRef<float> AttributeArrays::get_float(uint index) const
{
  BLI_assert(m_core.get_type(index) == AttributeType::Float);
  return ArrayRef<float>((float *)m_core.get_ptr(index) + m_start, m_size);
}

inline ArrayRef<float> AttributeArrays::get_float(StringRef name)
{
  return this->get_float(this->attribute_index(name));
}

inline ArrayRef<float3> AttributeArrays::get_float3(uint index) const
{
  BLI_assert(m_core.get_type(index) == AttributeType::Float3);
  return ArrayRef<float3>((float3 *)m_core.get_ptr(index) + m_start, m_size);
}

inline ArrayRef<float3> AttributeArrays::get_float3(StringRef name)
{
  return this->get_float3(this->attribute_index(name));
}

inline AttributeArrays AttributeArrays::slice(uint start, uint size) const
{
  return AttributeArrays(m_core, m_start + start, size);
}

inline AttributeArrays AttributeArrays::take_front(uint n) const
{
  BLI_assert(n <= m_size);
  return AttributeArrays(m_core, m_start, n);
}

}  // namespace BParticles
