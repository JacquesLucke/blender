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

class AttributeAccessor {
 private:
  const CPPType *cpp_type_;
  int64_t size_;

 public:
  AttributeAccessor(const CPPType &cpp_type, const int64_t size)
      : cpp_type_(&cpp_type), size_(size)
  {
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index < size_);
    this->access_single(index, r_value);
  }

  template<typename T> T get(const int64_t index) const
  {
    BLI_assert(index < size_);
    T value;
    value.~T();
    this->access_single(index, &value);
    return value;
  }

 protected:
  /* r_value is expected to be uninitialized. */
  virtual void access_single(const int64_t index, void *r_value) const = 0;
};

std::unique_ptr<AttributeAccessor> get_mesh_attribute_accessor(const MeshComponent &mesh_component,
                                                               const AttributeDomain domain,
                                                               const StringRef attribute_name);

}  // namespace blender::bke
