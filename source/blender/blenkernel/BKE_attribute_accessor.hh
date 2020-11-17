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
 */

#pragma once

#include "FN_cpp_type.hh"

#include "BKE_attribute.h"
#include "BKE_geometry_set.hh"

struct Mesh;

namespace blender::bke {

using fn::CPPType;

class ReadAttribute {
 protected:
  const AttributeDomain domain_;
  const CPPType &cpp_type_;
  const int64_t size_;

 public:
  ReadAttribute(AttributeDomain domain, const CPPType &cpp_type, const int64_t size)
      : domain_(domain), cpp_type_(cpp_type), size_(size)
  {
  }

  virtual ~ReadAttribute() = default;

  AttributeDomain domain() const
  {
    return domain_;
  }

  const CPPType &cpp_type() const
  {
    return cpp_type_;
  }

  int64_t size() const
  {
    return size_;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index < size_);
    this->get_internal(index, r_value);
  }

 protected:
  /* r_value is expected to be uninitialized. */
  virtual void get_internal(const int64_t index, void *r_value) const = 0;
};

using ReadAttributePtr = std::unique_ptr<ReadAttribute>;

template<typename T> class TypedReadAttribute {
 private:
  ReadAttributePtr attribute_;

 public:
  TypedReadAttribute(ReadAttributePtr attribute) : attribute_(std::move(attribute))
  {
    BLI_assert(attribute_);
    BLI_assert(attribute_->cpp_type().is<T>());
  }

  int64_t size() const
  {
    return attribute_->size();
  }

  T operator[](const int64_t index) const
  {
    BLI_assert(index < attribute_->size());
    T value;
    value.~T();
    attribute_->get(index, &value);
    return value;
  }
};

using FloatReadAttribute = TypedReadAttribute<float>;
using Float3ReadAttribute = TypedReadAttribute<float3>;

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name);

ReadAttributePtr mesh_attribute_adapt_domain(const MeshComponent &mesh_component,
                                             ReadAttributePtr attribute,
                                             const AttributeDomain to_domain);

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name,
                                             const CPPType &cpp_type,
                                             const AttributeDomain domain,
                                             const void *default_value = nullptr);

template<typename T>
TypedReadAttribute<T> mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                                  const StringRef attribute_name,
                                                  const AttributeDomain domain,
                                                  const T &default_value)
{
  ReadAttributePtr attribute = mesh_attribute_get_for_read(
      mesh_component,
      attribute_name,
      CPPType::get<T>(),
      domain,
      static_cast<const void *>(&default_value));
  BLI_assert(attribute);
  return attribute;
}

}  // namespace blender::bke
