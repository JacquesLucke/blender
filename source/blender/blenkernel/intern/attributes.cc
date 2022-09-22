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

Attribute::Attribute(const StringRef name,
                     const AttributeDomain domain,
                     const AttributeBaseType base_type,
                     const int array_size,
                     const int domain_size)
{
  memset(static_cast<void *>(this), 0, sizeof(*this));

  base_.name = BLI_strdupn(name.data(), name.size());
  base_.domain = domain;
  base_.base_type = base_type;
  base_.array_size = array_size;
  base_.domain_size = domain_size;

  BLI_assert(this->is_single());
}

Attribute::~Attribute()
{
  this->reset();
  MEM_freeN(base_.name);
  delete static_cast<AttributeRuntime *>(base_.runtime);
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

void Attribute::reset()
{
  switch (this->storage_type()) {
    case ATTR_STORAGE_TYPE_DENSE_ARRAY: {
      MEM_SAFE_FREE(base_.values);
      break;
    }
    case ATTR_STORAGE_TYPE_SPARSE_INDICES: {
      MEM_SAFE_FREE(base_.values);
      MEM_SAFE_FREE(base_.indices);
      MEM_SAFE_FREE(base_.fallback);
      base_.num_indices = 0;
      break;
    }
  }
  base_.storage_type = ATTR_STORAGE_TYPE_SPARSE_INDICES;
}

void Attribute::replace_with_dense(void *values)
{
  this->reset();
  base_.values = values;
  base_.storage_type = ATTR_STORAGE_TYPE_DENSE_ARRAY;
}

void Attribute::replace_with_sparse(void *values, MutableSpan<int> indices, void *fallback)
{
  BLI_assert(std::is_sorted(indices.begin(), indices.end()));
  this->reset();
  base_.values = values;
  base_.indices = indices.data();
  base_.num_indices = indices.size();
  base_.fallback = fallback;
  base_.storage_type = ATTR_STORAGE_TYPE_SPARSE_INDICES;
}

void Attribute::convert_to_dense()
{
  if (this->is_dense()) {
    return;
  }
  if (base_.domain_size == 0) {
    base_.storage_type = ATTR_STORAGE_TYPE_DENSE_ARRAY;
    return;
  }
  const GVArray old_values = this->base_values();
  const CPPType &cpp_type = this->base_cpp_type();
  const size_t buffer_size = size_t(base_.array_size) * size_t(base_.domain_size) *
                             size_t(cpp_type.size());
  const size_t buffer_alignment = size_t(cpp_type.alignment());
  void *buffer = MEM_mallocN_aligned(buffer_size, buffer_alignment, __func__);
  old_values.materialize_to_uninitialized(buffer);

  this->reset();

  base_.values = buffer;
  base_.storage_type = ATTR_STORAGE_TYPE_DENSE_ARRAY;
}

class GVArrayImpl_For_SparseIndicesAttribute : public GVArrayImpl {
 private:
  const Attribute *attribute_;

 public:
  GVArrayImpl_For_SparseIndicesAttribute(const Attribute &attribute)
      : GVArrayImpl(attribute.base_cpp_type(), attribute.domain_size() * attribute.array_size()),
        attribute_(&attribute)
  {
  }

  void get_to_uninitialized(const int64_t index, void *r_value) const override
  {
    /* TODO */
    UNUSED_VARS(index, r_value);
  }
};

GVArray Attribute::base_values() const
{
  const CPPType &cpp_type = this->base_cpp_type();
  if (base_.domain_size == 0) {
    return GVArray::ForEmpty(cpp_type);
  }
  switch (this->storage_type()) {
    case ATTR_STORAGE_TYPE_DENSE_ARRAY:
      return GVArray::ForSpan(this->dense_base_values());
    case ATTR_STORAGE_TYPE_SPARSE_INDICES:
      return GVArray::For<GVArrayImpl_For_SparseIndicesAttribute>(*this);
  }
  BLI_assert_unreachable();
  return {};
}

}  // namespace blender::bke
