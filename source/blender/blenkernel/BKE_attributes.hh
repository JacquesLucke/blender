/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_attributes.h"

#include "BLI_generic_pointer.hh"
#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_virtual_array.hh"

namespace blender::bke {

struct AttributeRuntime {
};

struct AttributesRuntime {
  Map<std::string, Attribute *> attribute_by_name;
};

struct AttributeElementRef {
  int attribute_index;
  void *value;
};

Array<Span<AttributeElementRef>> create_attributes_per_element_lookup(
    Span<const Attribute *> attributes, LinearAllocator<> &allocator);

class Attribute {
 private:
  ::Attribute base_;

 public:
  Attribute(StringRef name,
            AttributeDomain domain,
            AttributeBaseType base_type,
            int array_size,
            int domain_size);
  ~Attribute();

  AttributeStorageType storage_type() const;
  AttributeDomain domain() const;
  AttributeBaseType base_type() const;
  int array_size() const;
  int domain_size() const;
  const CPPType &base_cpp_type() const;

  template<typename T> VArray<T> values() const;
  GVArray values() const;

  bool is_dense() const;
  template<typename T> Span<T> dense_values() const;
  template<typename T> Span<T> dense_base_values() const;
  template<typename T> MutableSpan<T> dense_values_for_write();
  template<typename T> MutableSpan<T> dense_base_values_for_write();
  GSpan dense_base_values() const;
  GMutableSpan dense_base_values_for_write();

  bool is_sparse() const;
  Span<int> sparse_indices() const;
  template<typename T> Span<T> sparse_values() const;
  template<typename T> Span<T> sparse_base_values() const;
  template<typename T> MutableSpan<T> sparse_values_for_write();
  template<typename T> MutableSpan<T> sparse_base_values_for_write();
  GSpan sparse_base_values() const;
  GMutableSpan sparse_base_values_for_write();

  template<typename T> const T &sparse_fallback() const;
  template<typename T> T &sparse_fallback_for_write();
  GPointer sparse_base_fallback() const;
  GMutablePointer sparse_base_fallback_for_write();

  void convert_to_dense();
  void convert_to_sparse(void *fallback);

  void replace_with_dense(void *values);
  void replace_with_sparse(void *values, MutableSpan<int> indices, void *fallback);

  bool is_single() const;
  void reset();
};

class Attributes {
 private:
  ::Attributes base_;

 public:
  Attributes();
  Attributes(const Attributes &other);
  Attributes(Attributes &&other);
  ~Attributes();

  Attribute *lookup(StringRef name);
  Attribute &add(StringRef name,
                 AttributeDomain domain,
                 AttributeBaseType base_type,
                 int array_size,
                 int domain_size);
};

/* -------------------------------------------------------------------- */
/** \name #Attributes Inline Methods
 * \{ */

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Attribute Inline Methods
 * \{ */

inline AttributeStorageType Attribute::storage_type() const
{
  return AttributeStorageType(base_.storage_type);
}

inline AttributeDomain Attribute::domain() const
{
  return AttributeDomain(base_.domain);
}

inline AttributeBaseType Attribute::base_type() const
{
  return AttributeBaseType(base_.base_type);
}

inline int Attribute::array_size() const
{
  return base_.array_size;
}

inline int Attribute::domain_size() const
{
  return base_.domain_size;
}

inline bool Attribute::is_single() const
{
  return this->storage_type() == ATTR_STORAGE_TYPE_SPARSE_INDICES && base_.num_indices == 0;
}

inline bool Attribute::is_dense() const
{
  return this->storage_type() == ATTR_STORAGE_TYPE_DENSE_ARRAY;
}

inline bool Attribute::is_sparse() const
{
  return this->storage_type() == ATTR_STORAGE_TYPE_SPARSE_INDICES;
}

template<typename T> inline Span<T> Attribute::dense_values() const
{
  return this->dense_base_values<typename T::base_type>().template cast<T>();
}

template<typename T> inline Span<T> Attribute::dense_base_values() const
{
  return this->dense_base_values().typed<T>();
}

template<typename T> inline MutableSpan<T> Attribute::dense_values_for_write()
{
  return this->dense_base_values<typename T::base_type>().template cast<T>();
}

template<typename T> inline MutableSpan<T> Attribute::dense_base_values_for_write()
{
  return this->dense_base_values_for_write().typed<T>();
}

template<typename T> inline Span<T> Attribute::sparse_values() const
{
  return this->sparse_base_values<typename T::base_type>().template cast<T>();
}

template<typename T> inline Span<T> Attribute::sparse_base_values() const
{
  return this->sparse_base_values().typed<T>();
}

template<typename T> inline MutableSpan<T> Attribute::sparse_values_for_write()
{
  return this->sparse_values_for_write<typename T::base_type>().template cast<T>();
}

template<typename T> inline MutableSpan<T> Attribute::sparse_base_values_for_write()
{
  return this->sparse_base_values_for_write().typed<T>();
}

/** \} */

}  // namespace blender::bke

static_assert(sizeof(::Attribute) == sizeof(blender::bke::Attribute));
static_assert(sizeof(::Attributes) == sizeof(blender::bke::Attributes));

inline blender::bke::Attributes &Attributes::wrap()
{
  return *reinterpret_cast<blender::bke::Attributes *>(this);
}

inline const blender::bke::Attributes &Attributes::wrap() const
{
  return *reinterpret_cast<const blender::bke::Attributes *>(this);
}

inline blender::bke::Attribute &Attribute::wrap()
{
  return *reinterpret_cast<blender::bke::Attribute *>(this);
}

inline const blender::bke::Attribute &Attribute::wrap() const
{
  return *reinterpret_cast<const blender::bke::Attribute *>(this);
}
