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

#include "BKE_attribute_accessor.hh"
#include "BKE_deform.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_span.hh"

namespace blender::bke {

class VertexWeightReadAttribute final : public ReadAttribute {
 private:
  Span<MDeformVert> dverts_;
  int dvert_index_;

 public:
  VertexWeightReadAttribute(const MDeformVert *dverts, const int totvert, const int dvert_index)
      : ReadAttribute(ATTR_DOMAIN_VERTEX, CPPType::get<float>(), totvert),
        dverts_(dverts, totvert),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const MDeformVert &dvert = dverts_[index];
    for (const MDeformWeight &weight : Span(dvert.dw, dvert.totweight)) {
      if (weight.def_nr == dvert_index_) {
        *(float *)r_value = weight.weight;
        return;
      }
    }
    *(float *)r_value = 0.0f;
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
};

template<typename StructT, typename FuncT>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  using ElemT = decltype(std::declval<FuncT>()(std::declval<StructT>()));

  Span<StructT> data_;
  FuncT function_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data, FuncT function)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        function_(std::move(function))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = function_(struct_value);
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

  void get_internal(const int64_t UNUSED(index), void *r_value) const override
  {
    this->cpp_type_.copy_to_uninitialized(value_, r_value);
  }
};

static ReadAttributePtr get_custom_data_read_attribute(const CustomData &custom_data,
                                                       const int size,
                                                       const StringRef attribute_name,
                                                       const AttributeDomain domain)
{
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
      }
    }
  }
  return {};
}

static ReadAttributePtr get_mesh_read_attribute__corner(const MeshComponent &mesh_component,
                                                        const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_custom_data_read_attribute(
      mesh->ldata, mesh->totloop, attribute_name, ATTR_DOMAIN_CORNER);
}

static ReadAttributePtr get_mesh_read_attribute__vertex(const MeshComponent &mesh_component,
                                                        const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  if (attribute_name == "Position") {
    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    return std::make_unique<DerivedArrayReadAttribute<MVert, decltype(get_vertex_position)>>(
        ATTR_DOMAIN_VERTEX, Span(mesh->mvert, mesh->totvert), get_vertex_position);
  }

  const int vertex_group_index = mesh_component.vertex_group_index(attribute_name);
  if (vertex_group_index >= 0) {
    return std::make_unique<VertexWeightReadAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  return get_custom_data_read_attribute(
      mesh->vdata, mesh->totvert, attribute_name, ATTR_DOMAIN_VERTEX);
}

static ReadAttributePtr get_mesh_read_attribute__edge(const MeshComponent &mesh_component,
                                                      const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_custom_data_read_attribute(
      mesh->edata, mesh->totedge, attribute_name, ATTR_DOMAIN_EDGE);
}

static ReadAttributePtr get_mesh_read_attribute__polygon(const MeshComponent &mesh_component,
                                                         const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_custom_data_read_attribute(
      mesh->pdata, mesh->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
}

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name)
{
  ReadAttributePtr corner_level = get_mesh_read_attribute__corner(mesh_component, attribute_name);
  if (corner_level) {
    return corner_level;
  }

  ReadAttributePtr vertex_level = get_mesh_read_attribute__vertex(mesh_component, attribute_name);
  if (vertex_level) {
    return vertex_level;
  }

  ReadAttributePtr edge_level = get_mesh_read_attribute__edge(mesh_component, attribute_name);
  if (edge_level) {
    return edge_level;
  }

  ReadAttributePtr polygon_level = get_mesh_read_attribute__polygon(mesh_component,
                                                                    attribute_name);
  if (polygon_level) {
    return polygon_level;
  }

  return {};
}

static ReadAttributePtr adapt_mesh_attribute_to_corner(const MeshComponent &UNUSED(mesh_component),
                                                       ReadAttributePtr UNUSED(attribute))
{
  return {};
}

static ReadAttributePtr adapt_mesh_attribute_to_vertex(const MeshComponent &UNUSED(mesh_component),
                                                       ReadAttributePtr UNUSED(attribute))
{
  return {};
}

static ReadAttributePtr adapt_mesh_attribute_to_edge(const MeshComponent &UNUSED(mesh_component),
                                                     ReadAttributePtr UNUSED(attribute))
{
  return {};
}

static ReadAttributePtr adapt_mesh_attribute_to_polygon(
    const MeshComponent &UNUSED(mesh_component), ReadAttributePtr UNUSED(attribute))
{
  return {};
}

ReadAttributePtr mesh_attribute_adapt_domain(const MeshComponent &mesh_component,
                                             ReadAttributePtr attribute,
                                             const AttributeDomain to_domain)
{
  if (!attribute) {
    return {};
  }
  const AttributeDomain from_domain = attribute->domain();
  if (from_domain == to_domain) {
    return attribute;
  }

  switch (to_domain) {
    case ATTR_DOMAIN_CORNER:
      return adapt_mesh_attribute_to_corner(mesh_component, std::move(attribute));
    case ATTR_DOMAIN_VERTEX:
      return adapt_mesh_attribute_to_vertex(mesh_component, std::move(attribute));
    case ATTR_DOMAIN_EDGE:
      return adapt_mesh_attribute_to_edge(mesh_component, std::move(attribute));
    case ATTR_DOMAIN_POLYGON:
      return adapt_mesh_attribute_to_polygon(mesh_component, std::move(attribute));
    default:
      return {};
  }
}

static int get_domain_length(const MeshComponent &mesh_component, const AttributeDomain domain)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return 0;
  }
  switch (domain) {
    case ATTR_DOMAIN_CORNER:
      return mesh->totloop;
    case ATTR_DOMAIN_VERTEX:
      return mesh->totvert;
    case ATTR_DOMAIN_EDGE:
      return mesh->totedge;
    case ATTR_DOMAIN_POLYGON:
      return mesh->totpoly;
    default:
      break;
  }
  return 0;
}

static ReadAttributePtr make_default_attribute(const MeshComponent &mesh_component,
                                               const AttributeDomain domain,
                                               const CPPType &cpp_type,
                                               const void *default_value)
{

  const int length = get_domain_length(mesh_component, domain);
  return std::make_unique<ConstantReadAttribute>(domain, length, cpp_type, default_value);
}

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name,
                                             const AttributeDomain domain,
                                             const CPPType &cpp_type,
                                             const void *default_value)
{
  ReadAttributePtr attribute = mesh_attribute_get_for_read(mesh_component, attribute_name);
  auto get_default_or_empty = [&]() -> ReadAttributePtr {
    if (default_value != nullptr) {
      return make_default_attribute(mesh_component, domain, cpp_type, default_value);
    }
    return {};
  };

  if (!attribute) {
    return get_default_or_empty();
  }
  if (attribute->domain() != domain) {
    attribute = mesh_attribute_adapt_domain(mesh_component, std::move(attribute), domain);
  }
  if (!attribute) {
    return get_default_or_empty();
  }
  if (attribute->cpp_type() != cpp_type) {
    /* TODO: Support some type conversions. */
    return get_default_or_empty();
  }
  return attribute;
}

}  // namespace blender::bke
