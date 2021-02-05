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

#include <utility>

#include "BKE_attribute_access.hh"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_span.hh"

#include "CLG_log.h"

#include "NOD_node_tree_multi_function.hh"

static CLG_LogRef LOG = {"bke.attribute_access"};

using blender::float3;
using blender::Set;
using blender::StringRef;
using blender::StringRefNull;
using blender::bke::ReadAttributePtr;
using blender::bke::WriteAttributePtr;
using blender::fn::GMutableSpan;

/* Can't include BKE_object_deform.h right now, due to an enum forward declaration.  */
extern "C" MDeformVert *BKE_object_defgroup_data_create(ID *id);

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Attribute Accessor implementations
 * \{ */

ReadAttribute::~ReadAttribute()
{
  if (array_is_temporary_ && array_buffer_ != nullptr) {
    cpp_type_.destruct_n(array_buffer_, size_);
    MEM_freeN(array_buffer_);
  }
}

fn::GSpan ReadAttribute::get_span() const
{
  if (size_ == 0) {
    return fn::GSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    std::lock_guard lock{span_mutex_};
    if (array_buffer_ == nullptr) {
      this->initialize_span();
    }
  }
  return fn::GSpan(cpp_type_, array_buffer_, size_);
}

void ReadAttribute::initialize_span() const
{
  const int element_size = cpp_type_.size();
  array_buffer_ = MEM_mallocN_aligned(size_ * element_size, cpp_type_.alignment(), __func__);
  array_is_temporary_ = true;
  for (const int i : IndexRange(size_)) {
    this->get_internal(i, POINTER_OFFSET(array_buffer_, i * element_size));
  }
}

WriteAttribute::~WriteAttribute()
{
  if (array_should_be_applied_) {
    CLOG_ERROR(&LOG, "Forgot to call apply_span.");
  }
  if (array_is_temporary_ && array_buffer_ != nullptr) {
    cpp_type_.destruct_n(array_buffer_, size_);
    MEM_freeN(array_buffer_);
  }
}

/**
 * Get a mutable span that can be modified. When all modifications to the attribute are done,
 * #apply_span should be called. */
fn::GMutableSpan WriteAttribute::get_span()
{
  if (size_ == 0) {
    return fn::GMutableSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    this->initialize_span(false);
  }
  array_should_be_applied_ = true;
  return fn::GMutableSpan(cpp_type_, array_buffer_, size_);
}

fn::GMutableSpan WriteAttribute::get_span_for_write_only()
{
  if (size_ == 0) {
    return fn::GMutableSpan(cpp_type_);
  }
  if (array_buffer_ == nullptr) {
    this->initialize_span(true);
  }
  array_should_be_applied_ = true;
  return fn::GMutableSpan(cpp_type_, array_buffer_, size_);
}

void WriteAttribute::initialize_span(const bool write_only)
{
  const int element_size = cpp_type_.size();
  array_buffer_ = MEM_mallocN_aligned(element_size * size_, cpp_type_.alignment(), __func__);
  array_is_temporary_ = true;
  if (write_only) {
    /* This does nothing for trivial types, but is necessary for general correctness. */
    cpp_type_.construct_default_n(array_buffer_, size_);
  }
  else {
    for (const int i : IndexRange(size_)) {
      this->get(i, POINTER_OFFSET(array_buffer_, i * element_size));
    }
  }
}

void WriteAttribute::apply_span()
{
  this->apply_span_if_necessary();
  array_should_be_applied_ = false;
}

void WriteAttribute::apply_span_if_necessary()
{
  /* Only works when the span has been initialized beforehand. */
  BLI_assert(array_buffer_ != nullptr);

  const int element_size = cpp_type_.size();
  for (const int i : IndexRange(size_)) {
    this->set_internal(i, POINTER_OFFSET(array_buffer_, i * element_size));
  }
}

class VertexWeightWriteAttribute final : public WriteAttribute {
 private:
  MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightWriteAttribute(MDeformVert *dverts, const int totvert, const int dvert_index)
      : WriteAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    get_internal(dverts_, dvert_index_, index, r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    MDeformWeight *weight = BKE_defvert_ensure_index(&dverts_[index], dvert_index_);
    weight->weight = *reinterpret_cast<const float *>(value);
  }

  static void get_internal(const MDeformVert *dverts,
                           const int dvert_index,
                           const int64_t index,
                           void *r_value)
  {
    if (dverts == nullptr) {
      *(float *)r_value = 0.0f;
      return;
    }
    const MDeformVert &dvert = dverts[index];
    for (const MDeformWeight &weight : Span(dvert.dw, dvert.totweight)) {
      if (weight.def_nr == dvert_index) {
        *(float *)r_value = weight.weight;
        return;
      }
    }
    *(float *)r_value = 0.0f;
  }
};

class VertexWeightReadAttribute final : public ReadAttribute {
 private:
  const MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightReadAttribute(const MDeformVert *dverts, const int totvert, const int dvert_index)
      : ReadAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    VertexWeightWriteAttribute::get_internal(dverts_, dvert_index_, index, r_value);
  }
};

template<typename T> class ArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<T> data_;

 public:
  ArrayWriteAttribute(AttributeDomain domain, MutableSpan<T> data)
      : WriteAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data_[index] = *reinterpret_cast<const T *>(value);
  }

  void initialize_span(const bool UNUSED(write_only)) override
  {
    array_buffer_ = data_.data();
    array_is_temporary_ = false;
  }

  void apply_span_if_necessary() override
  {
    /* Do nothing, because the span contains the attribute itself already. */
  }
};

/* This is used by the #OutputAttributePtr class. */
class TemporaryWriteAttribute final : public WriteAttribute {
 public:
  GMutableSpan data;
  GeometryComponent &component;
  std::string final_name;

  TemporaryWriteAttribute(AttributeDomain domain,
                          GMutableSpan data,
                          GeometryComponent &component,
                          std::string final_name)
      : WriteAttribute(domain, data.type(), data.size()),
        data(data),
        component(component),
        final_name(std::move(final_name))
  {
  }

  ~TemporaryWriteAttribute() override
  {
    if (data.data() != nullptr) {
      cpp_type_.destruct_n(data.data(), data.size());
      MEM_freeN(data.data());
    }
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    data.type().copy_to_uninitialized(data[index], r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data.type().copy_to_initialized(value, data[index]);
  }

  void initialize_span(const bool UNUSED(write_only)) override
  {
    array_buffer_ = data.data();
    array_is_temporary_ = false;
  }

  void apply_span_if_necessary() override
  {
    /* Do nothing, because the span contains the attribute itself already. */
  }
};

template<typename T> class ArrayReadAttribute final : public ReadAttribute {
 private:
  Span<T> data_;

 public:
  ArrayReadAttribute(AttributeDomain domain, Span<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void initialize_span() const override
  {
    /* The data will not be modified, so this const_cast is fine. */
    array_buffer_ = const_cast<T *>(data_.data());
    array_is_temporary_ = false;
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, const ElemT &)>
class DerivedArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<StructT> data_;

 public:
  DerivedArrayWriteAttribute(AttributeDomain domain, MutableSpan<StructT> data)
      : WriteAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    StructT &struct_value = data_[index];
    const ElemT &typed_value = *reinterpret_cast<const ElemT *>(value);
    SetFunc(struct_value, typed_value);
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  Span<StructT> data_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = GetFunc(struct_value);
    new (r_value) ElemT(value);
  }
};

class ConstantReadAttribute final : public ReadAttribute {
 private:
  void *value_;

 public:
  ConstantReadAttribute(AttributeDomain domain,
                        const int64_t size,
                        const CPPType &type,
                        const void *value)
      : ReadAttribute(domain, type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_to_uninitialized(value, value_);
  }

  ~ConstantReadAttribute() override
  {
    this->cpp_type_.destruct(value_);
    MEM_freeN(value_);
  }

  void get_internal(const int64_t UNUSED(index), void *r_value) const override
  {
    this->cpp_type_.copy_to_uninitialized(value_, r_value);
  }

  void initialize_span() const override
  {
    const int element_size = cpp_type_.size();
    array_buffer_ = MEM_mallocN_aligned(size_ * element_size, cpp_type_.alignment(), __func__);
    array_is_temporary_ = true;
    cpp_type_.fill_uninitialized(value_, array_buffer_, size_);
  }
};

class ConvertedReadAttribute final : public ReadAttribute {
 private:
  const CPPType &from_type_;
  const CPPType &to_type_;
  ReadAttributePtr base_attribute_;
  const nodes::DataTypeConversions &conversions_;

  static constexpr int MaxValueSize = 64;
  static constexpr int MaxValueAlignment = 64;

 public:
  ConvertedReadAttribute(ReadAttributePtr base_attribute, const CPPType &to_type)
      : ReadAttribute(base_attribute->domain(), to_type, base_attribute->size()),
        from_type_(base_attribute->cpp_type()),
        to_type_(to_type),
        base_attribute_(std::move(base_attribute)),
        conversions_(nodes::get_implicit_type_conversions())
  {
    if (from_type_.size() > MaxValueSize || from_type_.alignment() > MaxValueAlignment) {
      throw std::runtime_error(
          "type is larger than expected, the buffer size has to be increased");
    }
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    AlignedBuffer<MaxValueSize, MaxValueAlignment> buffer;
    base_attribute_->get(index, buffer.ptr());
    conversions_.convert(from_type_, to_type_, buffer.ptr(), r_value);
  }
};

/** \} */

const blender::fn::CPPType *custom_data_type_to_cpp_type(const CustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return &CPPType::get<float>();
    case CD_PROP_FLOAT2:
      return &CPPType::get<float2>();
    case CD_PROP_FLOAT3:
      return &CPPType::get<float3>();
    case CD_PROP_INT32:
      return &CPPType::get<int>();
    case CD_PROP_COLOR:
      return &CPPType::get<Color4f>();
    case CD_PROP_BOOL:
      return &CPPType::get<bool>();
    default:
      return nullptr;
  }
  return nullptr;
}

CustomDataType cpp_type_to_custom_data_type(const blender::fn::CPPType &type)
{
  if (type.is<float>()) {
    return CD_PROP_FLOAT;
  }
  if (type.is<float2>()) {
    return CD_PROP_FLOAT2;
  }
  if (type.is<float3>()) {
    return CD_PROP_FLOAT3;
  }
  if (type.is<int>()) {
    return CD_PROP_INT32;
  }
  if (type.is<Color4f>()) {
    return CD_PROP_COLOR;
  }
  if (type.is<bool>()) {
    return CD_PROP_BOOL;
  }
  return static_cast<CustomDataType>(-1);
}

class BuiltinAttributeProvider {
 private:
  const std::string name_;
  const AttributeDomain domain_;
  const CustomDataType data_type_;

 public:
  BuiltinAttributeProvider(std::string name,
                           const AttributeDomain domain,
                           const CustomDataType data_type)
      : name_(std::move(name)), domain_(domain), data_type_(data_type)
  {
  }

  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component) const = 0;

  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component) const
  {
    UNUSED_VARS(component);
    return {};
  }

  virtual bool try_delete(GeometryComponent &component) const
  {
    UNUSED_VARS(component);
    return false;
  }

  virtual bool try_create(GeometryComponent &component) const
  {
    UNUSED_VARS(component);
    return false;
  }

  virtual bool exists(const GeometryComponent &component) const
  {
    UNUSED_VARS(component);
    return false;
  }

  StringRefNull name() const
  {
    return name_;
  }

  AttributeDomain domain() const
  {
    return domain_;
  }

  CustomDataType data_type() const
  {
    return data_type_;
  }
};

class DynamicAttributesProvider {
 public:
  virtual ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                            const StringRef attribute_name) const
  {
    UNUSED_VARS(component, attribute_name);
    return {};
  }

  virtual WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                              const StringRef attribute_name) const
  {
    UNUSED_VARS(component, attribute_name);
    return {};
  }

  virtual bool try_delete(GeometryComponent &component, const StringRef attribute_name) const
  {
    UNUSED_VARS(component, attribute_name);
    return false;
  }

  virtual bool try_create(GeometryComponent &component,
                          const StringRef attribute_name,
                          const AttributeDomain domain,
                          const CustomDataType data_type) const
  {
    UNUSED_VARS(component, attribute_name, domain, data_type);
    return false;
  }

  virtual void list(const GeometryComponent &component, Set<std::string> &r_names) const
  {
    UNUSED_VARS(component, r_names);
  }
};

class CustomDataAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 |
                                                   CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_INT32 |
                                                   CD_MASK_PROP_COLOR | CD_MASK_PROP_BOOL;
  using CustomDataGetter = const CustomData *(*)(const GeometryComponent &component);
  using UpdateAfterReferencedDataCopy = void (*)(GeometryComponent &component);
  const AttributeDomain domain_;
  const CustomDataGetter data_getter_;
  const UpdateAfterReferencedDataCopy update_after_referenced_data_copy_;

 public:
  CustomDataAttributeProvider(
      const AttributeDomain domain,
      const CustomDataGetter data_getter,
      const UpdateAfterReferencedDataCopy update_after_referenced_data_copy)
      : domain_(domain),
        data_getter_(data_getter),
        update_after_referenced_data_copy_(update_after_referenced_data_copy)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
  {
    const CustomData *custom_data = this->get_custom_data(component);
    if (custom_data == nullptr) {
      return {};
    }
    const int domain_size = component.attribute_domain_size(domain_);
    for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
      if (layer.name != attribute_name) {
        continue;
      }
      const CustomDataType data_type = (CustomDataType)layer.type;
      switch (data_type) {
        case CD_PROP_FLOAT:
          return this->layer_to_read_attribute<float>(layer, domain_size);
        case CD_PROP_FLOAT2:
          return this->layer_to_read_attribute<float2>(layer, domain_size);
        case CD_PROP_FLOAT3:
          return this->layer_to_read_attribute<float3>(layer, domain_size);
        case CD_PROP_INT32:
          return this->layer_to_read_attribute<int>(layer, domain_size);
        case CD_PROP_COLOR:
          return this->layer_to_read_attribute<Color4f>(layer, domain_size);
        case CD_PROP_BOOL:
          return this->layer_to_read_attribute<bool>(layer, domain_size);
        default:
          break;
      }
    }
    return {};
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
  {
    /* Todo: make sure that shared pointcloud is copied before taking the customdata. */
    CustomData *custom_data = this->get_custom_data(component);
    if (custom_data == nullptr) {
      return {};
    }
    const int domain_size = component.attribute_domain_size(domain_);
    for (CustomDataLayer &layer : MutableSpan(custom_data->layers, custom_data->totlayer)) {
      if (layer.name != attribute_name) {
        continue;
      }
      void *data_old = layer.data;
      void *data_new = CustomData_duplicate_referenced_layer_named(
          custom_data, layer.type, layer.name, domain_size);
      if (data_new != data_old) {
        update_after_referenced_data_copy_(component);
      }
      const CustomDataType data_type = (CustomDataType)layer.type;
      switch (data_type) {
        case CD_PROP_FLOAT:
          return this->layer_to_write_attribute<float>(layer, domain_size);
        case CD_PROP_FLOAT2:
          return this->layer_to_write_attribute<float2>(layer, domain_size);
        case CD_PROP_FLOAT3:
          return this->layer_to_write_attribute<float3>(layer, domain_size);
        case CD_PROP_INT32:
          return this->layer_to_write_attribute<int>(layer, domain_size);
        case CD_PROP_COLOR:
          return this->layer_to_write_attribute<Color4f>(layer, domain_size);
        case CD_PROP_BOOL:
          return this->layer_to_write_attribute<bool>(layer, domain_size);
        default:
          break;
      }
    }
    return {};
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    CustomData *custom_data = this->get_custom_data(component);
    if (custom_data == nullptr) {
      return false;
    }
    const int domain_size = component.attribute_domain_size(domain_);
    for (const int i : IndexRange(custom_data->totlayer)) {
      const CustomDataLayer &layer = custom_data->layers[i];
      if (this->type_is_supported((CustomDataType)layer.type) && layer.name == attribute_name) {
        CustomData_free_layer(custom_data, layer.type, domain_size, i);
        return true;
      }
    }
    return false;
  }

  bool try_create(GeometryComponent &component,
                  const StringRef attribute_name,
                  const AttributeDomain domain,
                  const CustomDataType data_type) const final
  {
    if (domain_ != domain) {
      return false;
    }
    CustomData *custom_data = this->get_custom_data(component);
    if (custom_data == nullptr) {
      return false;
    }
    for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
      if (layer.name == attribute_name) {
        return false;
      }
    }
    const int domain_size = component.attribute_domain_size(domain_);
    char attribute_name_c[MAX_NAME];
    attribute_name.copy(attribute_name_c);
    CustomData_add_layer_named(
        custom_data, data_type, CD_DEFAULT, nullptr, domain_size, attribute_name_c);
    return true;
  }

  void list(const GeometryComponent &component, Set<std::string> &r_names) const final
  {
    const CustomData *custom_data = this->get_custom_data(component);
    if (custom_data == nullptr) {
      return;
    }
    for (const CustomDataLayer &layer : Span(custom_data->layers, custom_data->totlayer)) {
      if (this->type_is_supported((CustomDataType)layer.type)) {
        r_names.add(layer.name);
      }
    }
  }

 private:
  template<typename T>
  ReadAttributePtr layer_to_read_attribute(const CustomDataLayer &layer,
                                           const int domain_size) const
  {
    return std::make_unique<ArrayReadAttribute<T>>(
        domain_, Span(static_cast<const T *>(layer.data), domain_size));
  }

  template<typename T>
  WriteAttributePtr layer_to_write_attribute(CustomDataLayer &layer, const int domain_size) const
  {
    return std::make_unique<ArrayWriteAttribute<T>>(
        domain_, MutableSpan(static_cast<T *>(layer.data), domain_size));
  }

  bool type_is_supported(CustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }

  const CustomData *get_custom_data(const GeometryComponent &component) const
  {
    return data_getter_(component);
  }

  CustomData *get_custom_data(GeometryComponent &component) const
  {
    return const_cast<CustomData *>(data_getter_(component));
  }
};

static Mesh *get_mesh_for_write(GeometryComponent &component)
{
  BLI_assert(component.type() == GeometryComponentType::Mesh);
  MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
  return mesh_component.get_for_write();
}

static const Mesh *get_mesh_for_read(const GeometryComponent &component)
{
  BLI_assert(component.type() == GeometryComponentType::Mesh);
  const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
  return mesh_component.get_for_read();
}

class MVertPositionAttributeProvider final : public BuiltinAttributeProvider {
 public:
  MVertPositionAttributeProvider(std::string name)
      : BuiltinAttributeProvider(std::move(name), ATTR_DOMAIN_POINT, CD_PROP_FLOAT3)
  {
  }

  ReadAttributePtr try_get_for_read(const GeometryComponent &component) const final
  {
    const Mesh *mesh = get_mesh_for_read(component);
    if (mesh == nullptr) {
      return {};
    }
    return std::make_unique<DerivedArrayReadAttribute<MVert, float3, get_vertex_position>>(
        ATTR_DOMAIN_POINT, Span(mesh->mvert, mesh->totvert));
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component) const final
  {
    Mesh *mesh = get_mesh_for_write(component);
    if (mesh == nullptr) {
      return {};
    }
    CustomData_duplicate_referenced_layer(&mesh->vdata, CD_MVERT, mesh->totvert);
    BKE_mesh_update_customdata_pointers(mesh, false);
    return std::make_unique<
        DerivedArrayWriteAttribute<MVert, float3, get_vertex_position, set_vertex_position>>(
        ATTR_DOMAIN_POINT, MutableSpan(mesh->mvert, mesh->totvert));
  }

  bool exists(const GeometryComponent &component) const final
  {
    const Mesh *mesh = get_mesh_for_read(component);
    return mesh != nullptr;
  }

  static float3 get_vertex_position(const MVert &vert)
  {
    return float3(vert.co);
  }

  static void set_vertex_position(MVert &vert, const float3 &position)
  {
    copy_v3_v3(vert.co, position);
  }
};

class MeshUVsAttributeProvider final : public DynamicAttributesProvider {
 public:
  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
  {
    const Mesh *mesh = get_mesh_for_read(component);
    if (mesh == nullptr) {
      return {};
    }
    for (const CustomDataLayer &layer : Span(mesh->ldata.layers, mesh->ldata.totlayer)) {
      if (layer.type == CD_MLOOPUV) {
        if (layer.name == attribute_name) {
          return std::make_unique<DerivedArrayReadAttribute<MLoopUV, float2, get_loop_uv>>(
              ATTR_DOMAIN_CORNER, Span(static_cast<const MLoopUV *>(layer.data), mesh->totloop));
        }
      }
    }
    return {};
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
  {
    Mesh *mesh = get_mesh_for_write(component);
    if (mesh == nullptr) {
      return {};
    }
    for (CustomDataLayer &layer : MutableSpan(mesh->ldata.layers, mesh->ldata.totlayer)) {
      if (layer.type == CD_MLOOPUV) {
        if (layer.name == attribute_name) {
          void *data_old = layer.data;
          void *data_new = CustomData_duplicate_referenced_layer_named(
              &mesh->ldata, CD_MLOOPUV, layer.name, mesh->totloop);
          if (data_old != data_new) {
            BKE_mesh_update_customdata_pointers(mesh, false);
          }
          return std::make_unique<
              DerivedArrayWriteAttribute<MLoopUV, float2, get_loop_uv, set_loop_uv>>(
              ATTR_DOMAIN_CORNER, MutableSpan(static_cast<MLoopUV *>(layer.data), mesh->totloop));
        }
      }
    }
    return {};
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    Mesh *mesh = get_mesh_for_write(component);
    if (mesh == nullptr) {
      return false;
    }
    for (const int i : IndexRange(mesh->ldata.totlayer)) {
      const CustomDataLayer &layer = mesh->ldata.layers[i];
      if (layer.type == CD_MLOOPUV && layer.name == attribute_name) {
        CustomData_free_layer(&mesh->ldata, CD_MLOOPUV, mesh->totloop, i);
        return true;
      }
    }
    return false;
  }

  void list(const GeometryComponent &component, Set<std::string> &r_names) const final
  {
    const Mesh *mesh = get_mesh_for_read(component);
    if (mesh == nullptr) {
      return;
    }
    for (const CustomDataLayer &layer : Span(mesh->ldata.layers, mesh->ldata.totlayer)) {
      if (layer.type == CD_MLOOPUV) {
        r_names.add(layer.name);
      }
    }
  }

  static float2 get_loop_uv(const MLoopUV &uv)
  {
    return float2(uv.uv);
  }

  static void set_loop_uv(MLoopUV &uv, const float2 &co)
  {
    copy_v2_v2(uv.uv, co);
  }
};

class VertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh == nullptr || mesh->dvert == nullptr) {
      static const float default_value = 0.0f;
      return std::make_unique<ConstantReadAttribute>(
          ATTR_DOMAIN_POINT, mesh->totvert, CPPType::get<float>(), &default_value);
    }
    return std::make_unique<VertexWeightReadAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return {};
    }
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh->dvert == nullptr) {
      BKE_object_defgroup_data_create(&mesh->id);
    }
    else {
      /* Copy the data layer if it is shared with some other mesh. */
      mesh->dvert = (MDeformVert *)CustomData_duplicate_referenced_layer(
          &mesh->vdata, CD_MDEFORMVERT, mesh->totvert);
    }
    return std::make_unique<blender::bke::VertexWeightWriteAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);

    const int vertex_group_index = mesh_component.vertex_group_names().pop_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return false;
    }
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return true;
    }
    if (mesh->dvert == nullptr) {
      return true;
    }
    for (MDeformVert &dvert : MutableSpan(mesh->dvert, mesh->totvert)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, vertex_group_index);
      BKE_defvert_remove_group(&dvert, weight);
    }
    return true;
  }

  void list(const GeometryComponent &component, Set<std::string> &r_names) const final
  {
    BLI_assert(component.type() == GeometryComponentType::Mesh);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    mesh_component.vertex_group_names().foreach_item(
        [&](StringRef name, const int vertex_group_index) {
          if (vertex_group_index >= 0) {
            r_names.add(name);
          }
        });
  }
};

class ComponentAttributeProviders {
 private:
  Map<std::string, const BuiltinAttributeProvider *> builtin_attribute_providers_;
  Vector<const DynamicAttributesProvider *> dynamic_attribute_providers_;

 public:
  ComponentAttributeProviders(Span<const BuiltinAttributeProvider *> builtin_attribute_providers,
                              Span<const DynamicAttributesProvider *> dynamic_attribute_providers)
      : dynamic_attribute_providers_(dynamic_attribute_providers)
  {
    for (const BuiltinAttributeProvider *provider : builtin_attribute_providers) {
      /* Use #add_new to make sure that no two builtin attributes have the same name. */
      builtin_attribute_providers_.add_new(provider->name(), provider);
    }
  }
};

static ComponentAttributeProviders create_attribute_providers_for_mesh_component()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    Mesh *mesh = get_mesh_for_write(component);
    if (mesh != nullptr) {
      BKE_mesh_update_customdata_pointers(mesh, true);
    }
  };
  static MVertPositionAttributeProvider position("position");
  static MeshUVsAttributeProvider uvs;
  static VertexGroupsAttributeProvider vertex_groups;
  static CustomDataAttributeProvider corner_custom_data(
      ATTR_DOMAIN_CORNER,
      [](const GeometryComponent &component) {
        const Mesh *mesh = get_mesh_for_read(component);
        return mesh ? &mesh->ldata : nullptr;
      },
      update_custom_data_pointers);
  static CustomDataAttributeProvider point_custom_data(
      ATTR_DOMAIN_POINT,
      [](const GeometryComponent &component) {
        const Mesh *mesh = get_mesh_for_read(component);
        return mesh ? &mesh->vdata : nullptr;
      },
      update_custom_data_pointers);
  static CustomDataAttributeProvider edge_custom_data(
      ATTR_DOMAIN_EDGE,
      [](const GeometryComponent &component) {
        const Mesh *mesh = get_mesh_for_read(component);
        return mesh ? &mesh->edata : nullptr;
      },
      update_custom_data_pointers);
  static CustomDataAttributeProvider polygon_custom_data(
      ATTR_DOMAIN_POLYGON,
      [](const GeometryComponent &component) {
        const Mesh *mesh = get_mesh_for_read(component);
        return mesh ? &mesh->pdata : nullptr;
      },
      update_custom_data_pointers);
  return ComponentAttributeProviders({&position},
                                     {&uvs,
                                      &corner_custom_data,
                                      &vertex_groups,
                                      &point_custom_data,
                                      &edge_custom_data,
                                      &polygon_custom_data});
}

static ComponentAttributeProviders create_attribute_providers_for_point_cloud()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    PointCloudComponent &pointcloud_component = static_cast<PointCloudComponent &>(component);
    PointCloud *pointcloud = pointcloud_component.get_for_write();
    if (pointcloud != nullptr) {
      BKE_pointcloud_update_customdata_pointers(pointcloud);
    }
  };
  static CustomDataAttributeProvider point_custom_data(
      ATTR_DOMAIN_POINT,
      [](const GeometryComponent &component) {
        const PointCloudComponent &pointcloud_component = static_cast<const PointCloudComponent &>(
            component);
        const PointCloud *pointcloud = pointcloud_component.get_for_read();
        return pointcloud ? &pointcloud->pdata : nullptr;
      },
      update_custom_data_pointers);
  return ComponentAttributeProviders({}, {&point_custom_data});
}

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Utilities for Accessing Attributes
 * \{ */

static ReadAttributePtr read_attribute_from_custom_data(const CustomData &custom_data,
                                                        const int size,
                                                        const StringRef attribute_name,
                                                        const AttributeDomain domain)
{
  using namespace blender;
  using namespace blender::bke;
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayReadAttribute<float>>(
              domain, Span(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayReadAttribute<float2>>(
              domain, Span(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayReadAttribute<float3>>(
              domain, Span(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayReadAttribute<int>>(
              domain, Span(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayReadAttribute<Color4f>>(
              domain, Span(static_cast<Color4f *>(layer.data), size));
        case CD_PROP_BOOL:
          return std::make_unique<ArrayReadAttribute<bool>>(
              domain, Span(static_cast<bool *>(layer.data), size));
        case CD_MLOOPUV:
          return std::make_unique<
              DerivedArrayReadAttribute<MLoopUV, float2, MeshUVsAttributeProvider::get_loop_uv>>(
              domain, Span(static_cast<MLoopUV *>(layer.data), size));
      }
    }
  }
  return {};
}

static WriteAttributePtr write_attribute_from_custom_data(
    CustomData &custom_data,
    const int size,
    const StringRef attribute_name,
    const AttributeDomain domain,
    const std::function<void()> &update_customdata_pointers)
{

  using namespace blender;
  using namespace blender::bke;
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      const void *data_before = layer.data;
      /* The data layer might be shared with someone else. Since the caller wants to modify it, we
       * copy it first. */
      CustomData_duplicate_referenced_layer_named(&custom_data, layer.type, layer.name, size);
      if (data_before != layer.data) {
        update_customdata_pointers();
      }
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayWriteAttribute<float>>(
              domain, MutableSpan(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayWriteAttribute<float2>>(
              domain, MutableSpan(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayWriteAttribute<float3>>(
              domain, MutableSpan(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayWriteAttribute<int>>(
              domain, MutableSpan(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayWriteAttribute<Color4f>>(
              domain, MutableSpan(static_cast<Color4f *>(layer.data), size));
        case CD_PROP_BOOL:
          return std::make_unique<ArrayWriteAttribute<bool>>(
              domain, MutableSpan(static_cast<bool *>(layer.data), size));
        case CD_MLOOPUV:
          return std::make_unique<
              DerivedArrayWriteAttribute<MLoopUV,
                                         float2,
                                         MeshUVsAttributeProvider::get_loop_uv,
                                         MeshUVsAttributeProvider::set_loop_uv>>(
              domain, MutableSpan(static_cast<MLoopUV *>(layer.data), size));
      }
    }
  }
  return {};
}

/* Returns true when the layer was found and is deleted. */
static bool delete_named_custom_data_layer(CustomData &custom_data,
                                           const StringRef attribute_name,
                                           const int size)
{
  for (const int index : blender::IndexRange(custom_data.totlayer)) {
    const CustomDataLayer &layer = custom_data.layers[index];
    if (layer.name == attribute_name) {
      CustomData_free_layer(&custom_data, layer.type, size, index);
      return true;
    }
  }
  return false;
}

static void get_custom_data_layer_attribute_names(const CustomData &custom_data,
                                                  const GeometryComponent &component,
                                                  const AttributeDomain domain,
                                                  Set<std::string> &r_names)
{
  for (const CustomDataLayer &layer : blender::Span(custom_data.layers, custom_data.totlayer)) {
    const CustomDataType data_type = static_cast<CustomDataType>(layer.type);
    if (component.attribute_domain_with_type_supported(domain, data_type) ||
        ELEM(data_type, CD_MLOOPUV)) {
      r_names.add(layer.name);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

bool GeometryComponent::attribute_domain_supported(const AttributeDomain UNUSED(domain)) const
{
  return false;
}

bool GeometryComponent::attribute_domain_with_type_supported(
    const AttributeDomain UNUSED(domain), const CustomDataType UNUSED(data_type)) const
{
  return false;
}

int GeometryComponent::attribute_domain_size(const AttributeDomain UNUSED(domain)) const
{
  BLI_assert(false);
  return 0;
}

bool GeometryComponent::attribute_is_builtin(const StringRef UNUSED(attribute_name)) const
{
  return true;
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(
    const StringRef UNUSED(attribute_name)) const
{
  return {};
}

ReadAttributePtr GeometryComponent::attribute_try_adapt_domain(ReadAttributePtr attribute,
                                                               const AttributeDomain domain) const
{
  if (attribute && attribute->domain() == domain) {
    return attribute;
  }
  return {};
}

WriteAttributePtr GeometryComponent::attribute_try_get_for_write(
    const StringRef UNUSED(attribute_name))
{
  return {};
}

bool GeometryComponent::attribute_try_delete(const StringRef UNUSED(attribute_name))
{
  return false;
}

bool GeometryComponent::attribute_try_create(const StringRef UNUSED(attribute_name),
                                             const AttributeDomain UNUSED(domain),
                                             const CustomDataType UNUSED(data_type))
{
  return false;
}

Set<std::string> GeometryComponent::attribute_names() const
{
  return {};
}

bool GeometryComponent::attribute_exists(const blender::StringRef attribute_name) const
{
  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (attribute) {
    return true;
  }
  return false;
}

static ReadAttributePtr try_adapt_data_type(ReadAttributePtr attribute,
                                            const blender::fn::CPPType &to_type)
{
  const blender::fn::CPPType &from_type = attribute->cpp_type();
  if (from_type == to_type) {
    return attribute;
  }

  const blender::nodes::DataTypeConversions &conversions =
      blender::nodes::get_implicit_type_conversions();
  if (!conversions.is_convertible(from_type, to_type)) {
    return {};
  }

  return std::make_unique<blender::bke::ConvertedReadAttribute>(std::move(attribute), to_type);
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(
    const StringRef attribute_name,
    const AttributeDomain domain,
    const CustomDataType data_type) const
{
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return {};
  }

  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (!attribute) {
    return {};
  }

  if (attribute->domain() != domain) {
    attribute = this->attribute_try_adapt_domain(std::move(attribute), domain);
    if (!attribute) {
      return {};
    }
  }

  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  if (attribute->cpp_type() != *cpp_type) {
    attribute = try_adapt_data_type(std::move(attribute), *cpp_type);
    if (!attribute) {
      return {};
    }
  }

  return attribute;
}

ReadAttributePtr GeometryComponent::attribute_try_get_for_read(const StringRef attribute_name,
                                                               const AttributeDomain domain) const
{
  if (!this->attribute_domain_supported(domain)) {
    return {};
  }

  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name);
  if (!attribute) {
    return {};
  }

  if (attribute->domain() != domain) {
    attribute = this->attribute_try_adapt_domain(std::move(attribute), domain);
    if (!attribute) {
      return {};
    }
  }

  return attribute;
}

ReadAttributePtr GeometryComponent::attribute_get_for_read(const StringRef attribute_name,
                                                           const AttributeDomain domain,
                                                           const CustomDataType data_type,
                                                           const void *default_value) const
{
  BLI_assert(this->attribute_domain_with_type_supported(domain, data_type));

  ReadAttributePtr attribute = this->attribute_try_get_for_read(attribute_name, domain, data_type);
  if (attribute) {
    return attribute;
  }
  return this->attribute_get_constant_for_read(domain, data_type, default_value);
}

blender::bke::ReadAttributePtr GeometryComponent::attribute_get_constant_for_read(
    const AttributeDomain domain, const CustomDataType data_type, const void *value) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);
  if (value == nullptr) {
    value = cpp_type->default_value();
  }
  const int domain_size = this->attribute_domain_size(domain);
  return std::make_unique<blender::bke::ConstantReadAttribute>(
      domain, domain_size, *cpp_type, value);
}

blender::bke::ReadAttributePtr GeometryComponent::attribute_get_constant_for_read_converted(
    const AttributeDomain domain,
    const CustomDataType in_data_type,
    const CustomDataType out_data_type,
    const void *value) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  if (value == nullptr || in_data_type == out_data_type) {
    return this->attribute_get_constant_for_read(domain, out_data_type, value);
  }

  const blender::fn::CPPType *in_cpp_type = blender::bke::custom_data_type_to_cpp_type(
      in_data_type);
  const blender::fn::CPPType *out_cpp_type = blender::bke::custom_data_type_to_cpp_type(
      out_data_type);
  BLI_assert(in_cpp_type != nullptr);
  BLI_assert(out_cpp_type != nullptr);

  const blender::nodes::DataTypeConversions &conversions =
      blender::nodes::get_implicit_type_conversions();
  BLI_assert(conversions.is_convertible(*in_cpp_type, *out_cpp_type));

  void *out_value = alloca(out_cpp_type->size());
  conversions.convert(*in_cpp_type, *out_cpp_type, value, out_value);

  const int domain_size = this->attribute_domain_size(domain);
  blender::bke::ReadAttributePtr attribute = std::make_unique<blender::bke::ConstantReadAttribute>(
      domain, domain_size, *out_cpp_type, out_value);

  out_cpp_type->destruct(out_value);
  return attribute;
}

OutputAttributePtr GeometryComponent::attribute_try_get_for_output(const StringRef attribute_name,
                                                                   const AttributeDomain domain,
                                                                   const CustomDataType data_type,
                                                                   const void *default_value)
{
  BLI_assert(this->attribute_domain_with_type_supported(domain, data_type));

  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  WriteAttributePtr attribute = this->attribute_try_get_for_write(attribute_name);

  /* If the attribute doesn't exist, make a new one with the correct type. */
  if (!attribute) {
    this->attribute_try_create(attribute_name, domain, data_type);
    attribute = this->attribute_try_get_for_write(attribute_name);
    if (default_value != nullptr) {
      void *data = attribute->get_span_for_write_only().data();
      cpp_type->fill_initialized(default_value, data, attribute->size());
      attribute->apply_span();
    }
    return OutputAttributePtr(std::move(attribute));
  }

  /* If an existing attribute has a matching domain and type, just use that. */
  if (attribute->domain() == domain && attribute->cpp_type() == *cpp_type) {
    return OutputAttributePtr(std::move(attribute));
  }

  /* Otherwise create a temporary buffer to use before saving the new attribute. */
  return OutputAttributePtr(*this, domain, attribute_name, data_type);
}

/* Construct from an attribute that already exists in the geometry component. */
OutputAttributePtr::OutputAttributePtr(WriteAttributePtr attribute)
    : attribute_(std::move(attribute))
{
}

/* Construct a temporary attribute that has to replace an existing one later on. */
OutputAttributePtr::OutputAttributePtr(GeometryComponent &component,
                                       AttributeDomain domain,
                                       std::string final_name,
                                       CustomDataType data_type)
{
  const blender::fn::CPPType *cpp_type = blender::bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  const int domain_size = component.attribute_domain_size(domain);
  void *buffer = MEM_malloc_arrayN(domain_size, cpp_type->size(), __func__);
  GMutableSpan new_span{*cpp_type, buffer, domain_size};

  /* Copy converted values from conflicting attribute, in case the value is read.
   * TODO: An optimization could be to not do this, when the caller says that the attribute will
   * only be written. */
  ReadAttributePtr src_attribute = component.attribute_get_for_read(
      final_name, domain, data_type, nullptr);
  for (const int i : blender::IndexRange(domain_size)) {
    src_attribute->get(i, new_span[i]);
  }

  attribute_ = std::make_unique<blender::bke::TemporaryWriteAttribute>(
      domain, new_span, component, std::move(final_name));
}

/* Store the computed attribute. If it was stored from the beginning already, nothing is done. This
 * might delete another attribute with the same name. */
void OutputAttributePtr::save()
{
  if (!attribute_) {
    CLOG_WARN(&LOG, "Trying to save an attribute that does not exist anymore.");
    return;
  }

  blender::bke::TemporaryWriteAttribute *attribute =
      dynamic_cast<blender::bke::TemporaryWriteAttribute *>(attribute_.get());

  if (attribute == nullptr) {
    /* The attribute is saved already. */
    attribute_.reset();
    return;
  }

  StringRefNull name = attribute->final_name;
  const blender::fn::CPPType &cpp_type = attribute->cpp_type();

  /* Delete an existing attribute with the same name if necessary. */
  attribute->component.attribute_try_delete(name);

  if (!attribute->component.attribute_try_create(
          name, attribute_->domain(), attribute_->custom_data_type())) {
    /* Cannot create the target attribute for some reason. */
    CLOG_WARN(&LOG,
              "Creating the '%s' attribute with type '%s' failed.",
              name.c_str(),
              cpp_type.name().c_str());
    attribute_.reset();
    return;
  }

  WriteAttributePtr new_attribute = attribute->component.attribute_try_get_for_write(name);

  GMutableSpan temp_span = attribute->data;
  GMutableSpan new_span = new_attribute->get_span_for_write_only();
  BLI_assert(temp_span.size() == new_span.size());

  /* Currently we copy over the attribute. In the future we want to reuse the buffer. */
  cpp_type.move_to_initialized_n(temp_span.data(), new_span.data(), new_span.size());
  new_attribute->apply_span();

  attribute_.reset();
}

OutputAttributePtr::~OutputAttributePtr()
{
  if (attribute_) {
    CLOG_ERROR(&LOG, "Forgot to call #save or #apply_span_and_save.");
  }
}

/* Utility function to call #apply_span and #save in the right order. */
void OutputAttributePtr::apply_span_and_save()
{
  BLI_assert(attribute_);
  attribute_->apply_span();
  this->save();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud Component
 * \{ */

bool PointCloudComponent::attribute_domain_supported(const AttributeDomain domain) const
{
  return domain == ATTR_DOMAIN_POINT;
}

bool PointCloudComponent::attribute_domain_with_type_supported(
    const AttributeDomain domain, const CustomDataType data_type) const
{
  return domain == ATTR_DOMAIN_POINT && ELEM(data_type,
                                             CD_PROP_BOOL,
                                             CD_PROP_FLOAT,
                                             CD_PROP_FLOAT2,
                                             CD_PROP_FLOAT3,
                                             CD_PROP_INT32,
                                             CD_PROP_COLOR);
}

int PointCloudComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(domain == ATTR_DOMAIN_POINT);
  UNUSED_VARS_NDEBUG(domain);
  if (pointcloud_ == nullptr) {
    return 0;
  }
  return pointcloud_->totpoint;
}

bool PointCloudComponent::attribute_is_builtin(const StringRef attribute_name) const
{
  return attribute_name == "position";
}

ReadAttributePtr PointCloudComponent::attribute_try_get_for_read(
    const StringRef attribute_name) const
{
  if (pointcloud_ == nullptr) {
    return {};
  }

  return read_attribute_from_custom_data(
      pointcloud_->pdata, pointcloud_->totpoint, attribute_name, ATTR_DOMAIN_POINT);
}

WriteAttributePtr PointCloudComponent::attribute_try_get_for_write(const StringRef attribute_name)
{
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return {};
  }

  return write_attribute_from_custom_data(
      pointcloud->pdata, pointcloud->totpoint, attribute_name, ATTR_DOMAIN_POINT, [&]() {
        BKE_pointcloud_update_customdata_pointers(pointcloud);
      });
}

bool PointCloudComponent::attribute_try_delete(const StringRef attribute_name)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return false;
  }
  delete_named_custom_data_layer(pointcloud->pdata, attribute_name, pointcloud->totpoint);
  return true;
}

static bool custom_data_has_layer_with_name(const CustomData &custom_data, const StringRef name)
{
  for (const CustomDataLayer &layer : blender::Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name == name) {
      return true;
    }
  }
  return false;
}

bool PointCloudComponent::attribute_try_create(const StringRef attribute_name,
                                               const AttributeDomain domain,
                                               const CustomDataType data_type)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return false;
  }
  PointCloud *pointcloud = this->get_for_write();
  if (pointcloud == nullptr) {
    return false;
  }
  if (custom_data_has_layer_with_name(pointcloud->pdata, attribute_name)) {
    return false;
  }

  char attribute_name_c[MAX_NAME];
  attribute_name.copy(attribute_name_c);
  CustomData_add_layer_named(
      &pointcloud->pdata, data_type, CD_DEFAULT, nullptr, pointcloud_->totpoint, attribute_name_c);
  return true;
}

Set<std::string> PointCloudComponent::attribute_names() const
{
  if (pointcloud_ == nullptr) {
    return {};
  }

  Set<std::string> names;
  get_custom_data_layer_attribute_names(pointcloud_->pdata, *this, ATTR_DOMAIN_POINT, names);
  return names;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Component
 * \{ */

bool MeshComponent::attribute_domain_supported(const AttributeDomain domain) const
{
  return ELEM(
      domain, ATTR_DOMAIN_CORNER, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_POLYGON);
}

bool MeshComponent::attribute_domain_with_type_supported(const AttributeDomain domain,
                                                         const CustomDataType data_type) const
{
  if (!this->attribute_domain_supported(domain)) {
    return false;
  }
  return ELEM(data_type,
              CD_PROP_BOOL,
              CD_PROP_FLOAT,
              CD_PROP_FLOAT2,
              CD_PROP_FLOAT3,
              CD_PROP_INT32,
              CD_PROP_COLOR);
}

int MeshComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  if (mesh_ == nullptr) {
    return 0;
  }
  switch (domain) {
    case ATTR_DOMAIN_CORNER:
      return mesh_->totloop;
    case ATTR_DOMAIN_POINT:
      return mesh_->totvert;
    case ATTR_DOMAIN_EDGE:
      return mesh_->totedge;
    case ATTR_DOMAIN_POLYGON:
      return mesh_->totpoly;
    default:
      BLI_assert(false);
      break;
  }
  return 0;
}

bool MeshComponent::attribute_is_builtin(const StringRef attribute_name) const
{
  return attribute_name == "position";
}

ReadAttributePtr MeshComponent::attribute_try_get_for_read(const StringRef attribute_name) const
{
  if (mesh_ == nullptr) {
    return {};
  }

  if (attribute_name == "position") {
    return std::make_unique<blender::bke::DerivedArrayReadAttribute<
        MVert,
        float3,
        blender::bke::MVertPositionAttributeProvider::get_vertex_position>>(
        ATTR_DOMAIN_POINT, blender::Span(mesh_->mvert, mesh_->totvert));
  }

  ReadAttributePtr corner_attribute = read_attribute_from_custom_data(
      mesh_->ldata, mesh_->totloop, attribute_name, ATTR_DOMAIN_CORNER);
  if (corner_attribute) {
    return corner_attribute;
  }

  const int vertex_group_index = vertex_group_names_.lookup_default(attribute_name, -1);
  if (vertex_group_index >= 0) {
    return std::make_unique<blender::bke::VertexWeightReadAttribute>(
        mesh_->dvert, mesh_->totvert, vertex_group_index);
  }

  ReadAttributePtr vertex_attribute = read_attribute_from_custom_data(
      mesh_->vdata, mesh_->totvert, attribute_name, ATTR_DOMAIN_POINT);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  ReadAttributePtr edge_attribute = read_attribute_from_custom_data(
      mesh_->edata, mesh_->totedge, attribute_name, ATTR_DOMAIN_EDGE);
  if (edge_attribute) {
    return edge_attribute;
  }

  ReadAttributePtr polygon_attribute = read_attribute_from_custom_data(
      mesh_->pdata, mesh_->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

WriteAttributePtr MeshComponent::attribute_try_get_for_write(const StringRef attribute_name)
{
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return {};
  }

  const std::function<void()> update_mesh_pointers = [&]() {
    BKE_mesh_update_customdata_pointers(mesh, false);
  };

  if (attribute_name == "position") {
    CustomData_duplicate_referenced_layer(&mesh->vdata, CD_MVERT, mesh->totvert);
    update_mesh_pointers();

    return std::make_unique<blender::bke::DerivedArrayWriteAttribute<
        MVert,
        float3,
        blender::bke::MVertPositionAttributeProvider::get_vertex_position,
        blender::bke::MVertPositionAttributeProvider::set_vertex_position>>(
        ATTR_DOMAIN_POINT, blender::MutableSpan(mesh_->mvert, mesh_->totvert));
  }

  WriteAttributePtr corner_attribute = write_attribute_from_custom_data(
      mesh_->ldata, mesh_->totloop, attribute_name, ATTR_DOMAIN_CORNER, update_mesh_pointers);
  if (corner_attribute) {
    return corner_attribute;
  }

  const int vertex_group_index = vertex_group_names_.lookup_default_as(attribute_name, -1);
  if (vertex_group_index >= 0) {
    if (mesh_->dvert == nullptr) {
      BKE_object_defgroup_data_create(&mesh_->id);
    }
    else {
      /* Copy the data layer if it is shared with some other mesh. */
      mesh_->dvert = (MDeformVert *)CustomData_duplicate_referenced_layer(
          &mesh_->vdata, CD_MDEFORMVERT, mesh_->totvert);
    }
    return std::make_unique<blender::bke::VertexWeightWriteAttribute>(
        mesh_->dvert, mesh_->totvert, vertex_group_index);
  }

  WriteAttributePtr vertex_attribute = write_attribute_from_custom_data(
      mesh_->vdata, mesh_->totvert, attribute_name, ATTR_DOMAIN_POINT, update_mesh_pointers);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  WriteAttributePtr edge_attribute = write_attribute_from_custom_data(
      mesh_->edata, mesh_->totedge, attribute_name, ATTR_DOMAIN_EDGE, update_mesh_pointers);
  if (edge_attribute) {
    return edge_attribute;
  }

  WriteAttributePtr polygon_attribute = write_attribute_from_custom_data(
      mesh_->pdata, mesh_->totpoly, attribute_name, ATTR_DOMAIN_POLYGON, update_mesh_pointers);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

bool MeshComponent::attribute_try_delete(const StringRef attribute_name)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return false;
  }

  delete_named_custom_data_layer(mesh_->ldata, attribute_name, mesh_->totloop);
  delete_named_custom_data_layer(mesh_->vdata, attribute_name, mesh_->totvert);
  delete_named_custom_data_layer(mesh_->edata, attribute_name, mesh_->totedge);
  delete_named_custom_data_layer(mesh_->pdata, attribute_name, mesh_->totpoly);

  const int vertex_group_index = vertex_group_names_.lookup_default_as(attribute_name, -1);
  if (vertex_group_index != -1) {
    for (MDeformVert &dvert : blender::MutableSpan(mesh_->dvert, mesh_->totvert)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, vertex_group_index);
      BKE_defvert_remove_group(&dvert, weight);
    }
    vertex_group_names_.remove_as(attribute_name);
  }

  return true;
}

bool MeshComponent::attribute_try_create(const StringRef attribute_name,
                                         const AttributeDomain domain,
                                         const CustomDataType data_type)
{
  if (this->attribute_is_builtin(attribute_name)) {
    return false;
  }
  if (!this->attribute_domain_with_type_supported(domain, data_type)) {
    return false;
  }
  Mesh *mesh = this->get_for_write();
  if (mesh == nullptr) {
    return false;
  }

  char attribute_name_c[MAX_NAME];
  attribute_name.copy(attribute_name_c);

  switch (domain) {
    case ATTR_DOMAIN_CORNER: {
      if (custom_data_has_layer_with_name(mesh->ldata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->ldata, data_type, CD_DEFAULT, nullptr, mesh->totloop, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_POINT: {
      if (custom_data_has_layer_with_name(mesh->vdata, attribute_name)) {
        return false;
      }
      if (vertex_group_names_.contains_as(attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->vdata, data_type, CD_DEFAULT, nullptr, mesh->totvert, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_EDGE: {
      if (custom_data_has_layer_with_name(mesh->edata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->edata, data_type, CD_DEFAULT, nullptr, mesh->totedge, attribute_name_c);
      return true;
    }
    case ATTR_DOMAIN_POLYGON: {
      if (custom_data_has_layer_with_name(mesh->pdata, attribute_name)) {
        return false;
      }
      CustomData_add_layer_named(
          &mesh->pdata, data_type, CD_DEFAULT, nullptr, mesh->totpoly, attribute_name_c);
      return true;
    }
    default:
      return false;
  }
}

Set<std::string> MeshComponent::attribute_names() const
{
  if (mesh_ == nullptr) {
    return {};
  }

  Set<std::string> names;
  names.add("position");
  for (StringRef name : vertex_group_names_.keys()) {
    names.add(name);
  }
  get_custom_data_layer_attribute_names(mesh_->ldata, *this, ATTR_DOMAIN_CORNER, names);
  get_custom_data_layer_attribute_names(mesh_->vdata, *this, ATTR_DOMAIN_POINT, names);
  get_custom_data_layer_attribute_names(mesh_->edata, *this, ATTR_DOMAIN_EDGE, names);
  get_custom_data_layer_attribute_names(mesh_->pdata, *this, ATTR_DOMAIN_POLYGON, names);
  return names;
}

/** \} */
