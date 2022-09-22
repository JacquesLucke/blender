/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attributes.hh"

namespace blender::bke {

static const CPPType &base_type_to_cpp_type(const AttributeBaseType base_type)
{
  switch (base_type) {
    case ATTR_DATA_TYPE_FLOAT:
      return CPPType::get<float>();
    case ATTR_DATA_TYPE_DOUBLE:
      return CPPType::get<double>();
    case ATTR_DATA_TYPE_INT8:
      return CPPType::get<int8_t>();
    case ATTR_DATA_TYPE_INT16:
      return CPPType::get<int16_t>();
    case ATTR_DATA_TYPE_INT32:
      return CPPType::get<int32_t>();
    case ATTR_DATA_TYPE_INT64:
      return CPPType::get<int64_t>();
  }
  BLI_assert_unreachable();
  return CPPType::get<float>();
}

const CPPType &Attribute::base_cpp_type() const
{
  return base_type_to_cpp_type(this->base_type());
}

GSpan Attribute::dense_base_values() const
{
  BLI_assert(this->is_dense());
  const CPPType &type = this->base_cpp_type();
  return GSpan{type, base_.values, base_.array_size * base_.domain_size};
}

GMutableSpan Attribute::dense_base_values_for_write()
{
  BLI_assert(this->is_dense());
  const CPPType &type = this->base_cpp_type();
  return GMutableSpan{type, base_.values, base_.array_size * base_.domain_size};
}

Span<int> Attribute::sparse_indices() const
{
  BLI_assert(this->is_sparse());
  return Span<int>(base_.indices, base_.num_indices);
}

GSpan Attribute::sparse_base_values() const
{
  BLI_assert(this->is_sparse());
  const CPPType &type = this->base_cpp_type();
  return GSpan{type, base_.values, base_.num_indices};
}

GMutableSpan Attribute::sparse_base_values_for_write()
{
  BLI_assert(this->is_sparse());
  const CPPType &type = this->base_cpp_type();
  return GMutableSpan{type, base_.values, base_.num_indices};
}

}  // namespace blender::bke
