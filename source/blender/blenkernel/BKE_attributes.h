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
  AttributeStorageType storage_type() const;
  AttributeDomain domain() const;
  AttributeBaseType base_type() const;
  int array_size() const;
  int domain_size() const;

  template<typename T> VArray<T> values() const;
  GVArray values() const;

  template<typename T> Span<T> dense_values() const;
  template<typename T> Span<T> dense_base_values() const;
  template<typename T> MutableSpan<T> dense_values_for_write();
  template<typename T> MutableSpan<T> dense_base_values_for_write() const;
  GSpan dense_base_values() const;
  GMutableSpan dense_base_values_for_write();

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

  void replace_with_dense(void *data);
  void replace_with_sparse(void *data, MutableSpan<int> indices, void *fallback);

  bool is_single() const;
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

}  // namespace blender::bke

static_assert(sizeof(::Attribute) == sizeof(blender::bke::Attribute));
static_assert(sizeof(::Attributes) == sizeof(blender::bke::Attributes));
