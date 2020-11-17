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

class VertexWeightAttributeAccessor final : public AttributeAccessor {
 private:
  Span<MDeformVert> dverts_;
  int dvert_index_;

 public:
  VertexWeightAttributeAccessor(const MDeformVert *dverts,
                                const int totvert,
                                const int dvert_index)
      : AttributeAccessor(ATTR_DOMAIN_VERTEX, CPPType::get<float>(), totvert),
        dverts_(dverts, totvert),
        dvert_index_(dvert_index)
  {
  }

  void access_single(const int64_t index, void *r_value) const override
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

template<typename T> class ArrayAttributeAccessor final : public AttributeAccessor {
 private:
  Span<T> data_;

 public:
  ArrayAttributeAccessor(AttributeDomain domain, Span<T> data)
      : AttributeAccessor(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void access_single(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }
};

template<typename StructT, typename FuncT>
class DerivedArrayAttributeAccessor final : public AttributeAccessor {
 private:
  using ElemT = decltype(std::declval<FuncT>()(std::declval<StructT>()));

  Span<StructT> data_;
  FuncT function_;

 public:
  DerivedArrayAttributeAccessor(AttributeDomain domain, Span<StructT> data, FuncT function)
      : AttributeAccessor(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        function_(std::move(function))
  {
  }

  void access_single(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = function_(struct_value);
    new (r_value) ElemT(value);
  }
};

template<typename T> class ConstantAttributeAccessor final : public AttributeAccessor {
 private:
  T value_;

 public:
  ConstantAttributeAccessor(AttributeDomain domain, T value, const int64_t size)
      : AttributeAccessor(domain, CPPType::get<T>(), size), value_(std::move(value))
  {
  }

  void access_single(const int64_t index, void *r_value) const override
  {
    new (r_value) T(value_);
  }
};

class VertexToEdgeAccessor final : public AttributeAccessor {
 private:
  AttributeAccessorPtr vertex_accessor_;
  Span<MEdge> edges_;

 public:
  VertexToEdgeAccessor(AttributeAccessorPtr vertex_accessor, Span<MEdge> edges)
      : AttributeAccessor(ATTR_DOMAIN_EDGE, vertex_accessor->cpp_type(), edges.size()),
        vertex_accessor_(std::move(vertex_accessor)),
        edges_(edges)
  {
  }

  void access_single(const int64_t index, void *r_value) const override
  {
    const MEdge &edge = edges_[index];
    /* TODO: Interpolation. */
    vertex_accessor_->get(edge.v1, r_value);
  }
};

class VertexToCornerAccessor final : public AttributeAccessor {
 private:
  AttributeAccessorPtr vertex_accessor_;
  Span<MLoop> loops_;

 public:
  VertexToCornerAccessor(AttributeAccessorPtr vertex_accessor, Span<MLoop> loops)
      : AttributeAccessor(ATTR_DOMAIN_CORNER, vertex_accessor->cpp_type(), loops.size()),
        vertex_accessor_(std::move(vertex_accessor)),
        loops_(loops)
  {
  }

  void access_single(const int64_t index, void *r_value) const override
  {
    const MLoop &loop = loops_[index];
    vertex_accessor_->get(loop.v, r_value);
  }
};

static AttributeAccessorPtr get_attribute_layer_accessor(const CustomData &custom_data,
                                                         const int size,
                                                         const StringRef attribute_name,
                                                         const AttributeDomain domain)
{
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayAttributeAccessor<float>>(
              domain, Span(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayAttributeAccessor<float2>>(
              domain, Span(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayAttributeAccessor<float3>>(
              domain, Span(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayAttributeAccessor<int>>(
              domain, Span(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayAttributeAccessor<Color4f>>(
              domain, Span(static_cast<Color4f *>(layer.data), size));
      }
    }
  }
  return {};
}

static AttributeAccessorPtr get_mesh_attribute_accessor__corner(
    const MeshComponent &mesh_component, const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_attribute_layer_accessor(
      mesh->ldata, mesh->totloop, attribute_name, ATTR_DOMAIN_CORNER);
}

static AttributeAccessorPtr get_mesh_attribute_accessor__vertex(
    const MeshComponent &mesh_component, const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  if (attribute_name == "Position") {
    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    return std::make_unique<DerivedArrayAttributeAccessor<MVert, decltype(get_vertex_position)>>(
        ATTR_DOMAIN_VERTEX, Span(mesh->mvert, mesh->totvert), get_vertex_position);
  }

  const int vertex_group_index = mesh_component.vertex_group_index(attribute_name);
  if (vertex_group_index >= 0) {
    return std::make_unique<VertexWeightAttributeAccessor>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  return get_attribute_layer_accessor(
      mesh->vdata, mesh->totvert, attribute_name, ATTR_DOMAIN_VERTEX);
}

static AttributeAccessorPtr get_mesh_attribute_accessor__edge(const MeshComponent &mesh_component,
                                                              const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_attribute_layer_accessor(
      mesh->edata, mesh->totedge, attribute_name, ATTR_DOMAIN_EDGE);
}

static AttributeAccessorPtr get_mesh_attribute_accessor__polygon(
    const MeshComponent &mesh_component, const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return get_attribute_layer_accessor(
      mesh->pdata, mesh->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
}

AttributeAccessorPtr get_raw_mesh_attribute_accessor(const MeshComponent &mesh_component,
                                                     const StringRef attribute_name)
{
  AttributeAccessorPtr corner_level = get_mesh_attribute_accessor__corner(mesh_component,
                                                                          attribute_name);
  if (corner_level) {
    return corner_level;
  }

  AttributeAccessorPtr vertex_level = get_mesh_attribute_accessor__vertex(mesh_component,
                                                                          attribute_name);
  if (vertex_level) {
    return vertex_level;
  }

  AttributeAccessorPtr edge_level = get_mesh_attribute_accessor__edge(mesh_component,
                                                                      attribute_name);
  if (edge_level) {
    return edge_level;
  }

  AttributeAccessorPtr polygon_level = get_mesh_attribute_accessor__polygon(mesh_component,
                                                                            attribute_name);
  if (polygon_level) {
    return polygon_level;
  }

  return {};
}

static AttributeAccessorPtr adapt_mesh_attribute_accessor_to_corner(
    const MeshComponent &mesh_component, AttributeAccessorPtr attribute_accessor)
{
  const Mesh &mesh = *mesh_component.get_for_read();

  const AttributeDomain from_domain = attribute_accessor->domain();
  switch (from_domain) {
    case ATTR_DOMAIN_VERTEX:
      return std::make_unique<VertexToCornerAccessor>(std::move(attribute_accessor),
                                                      Span(mesh.mloop, mesh.totloop));
    case ATTR_DOMAIN_EDGE: {
      BLI_assert(!"currently not implemented");
      break;
    }
    case ATTR_DOMAIN_POLYGON: {
      BLI_assert(!"currently not implemented");
      break;
    }
    default: {
      break;
    }
  }
  return {};
}

static AttributeAccessorPtr adapt_mesh_attribute_accessor_to_vertex(
    const MeshComponent &UNUSED(mesh_component), AttributeAccessorPtr UNUSED(attribute_accessor))
{
  BLI_assert(!"currently not implemented");
  return {};
}

static AttributeAccessorPtr adapt_mesh_attribute_accessor_to_edge(
    const MeshComponent &UNUSED(mesh_component), AttributeAccessorPtr UNUSED(attribute_accessor))
{
  BLI_assert(!"currently not implemented");
  return {};
}

static AttributeAccessorPtr adapt_mesh_attribute_accessor_to_polygon(
    const MeshComponent &UNUSED(mesh_component), AttributeAccessorPtr UNUSED(attribute_accessor))
{
  BLI_assert(!"currently not implemented");
  return {};
}

AttributeAccessorPtr adapt_mesh_attribute_accessor_domain(const MeshComponent &mesh_component,
                                                          AttributeAccessorPtr attribute_accessor,
                                                          const AttributeDomain to_domain)
{
  if (!attribute_accessor) {
    return {};
  }
  const AttributeDomain from_domain = attribute_accessor->domain();
  if (from_domain == to_domain) {
    return attribute_accessor;
  }

  switch (to_domain) {
    case ATTR_DOMAIN_CORNER:
      return adapt_mesh_attribute_accessor_to_corner(mesh_component,
                                                     std::move(attribute_accessor));
    case ATTR_DOMAIN_VERTEX:
      return adapt_mesh_attribute_accessor_to_vertex(mesh_component,
                                                     std::move(attribute_accessor));
    case ATTR_DOMAIN_EDGE:
      return adapt_mesh_attribute_accessor_to_edge(mesh_component, std::move(attribute_accessor));
    case ATTR_DOMAIN_POLYGON:
      return adapt_mesh_attribute_accessor_to_polygon(mesh_component,
                                                      std::move(attribute_accessor));
    default:
      return {};
  }
}

}  // namespace blender::bke
