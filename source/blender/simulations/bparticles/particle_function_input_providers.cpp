#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"
#include "BLI_hash.h"

#include "particle_function_input_providers.hpp"

namespace BParticles {

using BLI::float2;
using BLI::rgba_b;
using BLI::rgba_f;

Optional<ParticleFunctionInputArray> AttributeInputProvider::get(InputProviderInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  uint element_size = m_type.size();
  int attribute_index = attributes.info().index_of_try(m_name, m_type);

  if (attribute_index == -1) {
    return {};
  }
  else {
    void *buffer = attributes.get(attribute_index).buffer();
    return ParticleFunctionInputArray(buffer, element_size, false);
  }
}

Optional<ParticleFunctionInputArray> SurfaceNormalInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  auto *surface_info = dynamic_cast<MeshSurfaceContext *>(action_context);
  if (surface_info == nullptr) {
    return {};
  }

  return ParticleFunctionInputArray(surface_info->world_normals(), false);
}

Optional<ParticleFunctionInputArray> SurfaceVelocityInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  auto *surface_info = dynamic_cast<MeshSurfaceContext *>(action_context);
  if (surface_info == nullptr) {
    return {};
  }

  return ParticleFunctionInputArray(surface_info->world_surface_velicities(), false);
}

Optional<ParticleFunctionInputArray> AgeInputProvider::get(InputProviderInterface &interface)
{
  auto birth_times = interface.attributes().get<float>("Birth Time");
  auto ages = BLI::temporary_allocate_array<float>(birth_times.size());

  ParticleTimes &times = interface.particle_times();
  if (times.type() == ParticleTimes::Type::Current) {
    auto current_times = times.current_times();
    for (uint pindex : interface.pindices()) {
      ages[pindex] = current_times[pindex] - birth_times[pindex];
    }
  }
  else if (times.type() == ParticleTimes::Type::DurationAndEnd) {
    auto remaining_durations = times.remaining_durations();
    float end_time = times.end_time();
    for (uint pindex : interface.pindices()) {
      ages[pindex] = end_time - remaining_durations[pindex] - birth_times[pindex];
    }
  }
  else {
    BLI_assert(false);
  }
  return ParticleFunctionInputArray(ages.as_ref(), true);
}

SurfaceImageInputProvider::SurfaceImageInputProvider(Image *image,
                                                     Optional<std::string> uv_map_name)
    : m_image(image), m_uv_map_name(std::move(uv_map_name))
{
  memset(&m_image_user, 0, sizeof(ImageUser));
  m_image_user.ok = true;
  m_ibuf = BKE_image_acquire_ibuf(image, &m_image_user, NULL);
  BLI_assert(m_ibuf);
}

SurfaceImageInputProvider::~SurfaceImageInputProvider()
{
  BKE_image_release_ibuf(m_image, m_ibuf, NULL);
}

Optional<ParticleFunctionInputArray> SurfaceImageInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  if (dynamic_cast<MeshSurfaceContext *>(action_context)) {
    auto *surface_info = dynamic_cast<MeshSurfaceContext *>(action_context);
    return this->compute_colors(
        interface, surface_info, IndexRange(interface.attributes().size()).as_array_ref());
  }
  else if (dynamic_cast<SourceParticleActionContext *>(action_context)) {
    auto *source = dynamic_cast<SourceParticleActionContext *>(action_context);
    auto *surface_info = dynamic_cast<MeshSurfaceContext *>(source->source_context());
    if (surface_info != nullptr) {
      return this->compute_colors(interface, surface_info, source->source_indices());
    }
  }
  return {};
}

static int find_uv_layer_index(Mesh *mesh, const Optional<std::string> &uv_map_name)
{
  if (uv_map_name.has_value()) {
    return CustomData_get_named_layer_index(&mesh->ldata, CD_MLOOPUV, uv_map_name.value().data());
  }
  else {
    return CustomData_get_active_layer(&mesh->ldata, CD_MLOOPUV);
  }
}

Optional<ParticleFunctionInputArray> SurfaceImageInputProvider::compute_colors(
    InputProviderInterface &interface,
    MeshSurfaceContext *surface_info,
    ArrayRef<uint> surface_info_mapping)
{
  const Object *object = surface_info->object();
  Mesh *mesh = (Mesh *)object->data;

  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
  ArrayRef<float3> barycentric_coords = surface_info->barycentric_coords();

  int uv_layer_index = find_uv_layer_index(mesh, m_uv_map_name);
  if (uv_layer_index < 0) {
    return {};
  }
  ArrayRef<MLoopUV> uv_layer = BLI::ref_c_array(
      (MLoopUV *)CustomData_get_layer_n(&mesh->ldata, CD_MLOOPUV, uv_layer_index), mesh->totloop);

  ArrayRef<rgba_b> pixel_buffer = BLI::ref_c_array((rgba_b *)m_ibuf->rect, m_ibuf->x * m_ibuf->y);

  uint size = interface.attributes().size();
  auto colors = BLI::temporary_allocate_array<rgba_f>(size);

  for (uint pindex : interface.pindices()) {
    uint source_index = surface_info_mapping[pindex];
    uint triangle_index = surface_info->looptri_indices()[source_index];
    const MLoopTri &triangle = triangles[triangle_index];

    float2 uv1 = uv_layer[triangle.tri[0]].uv;
    float2 uv2 = uv_layer[triangle.tri[1]].uv;
    float2 uv3 = uv_layer[triangle.tri[2]].uv;

    float2 uv;
    float3 vertex_weights = barycentric_coords[source_index];
    interp_v2_v2v2v2(uv, uv1, uv2, uv3, vertex_weights);

    uv = uv.clamped_01();
    uint x = uv.x * (m_ibuf->x - 1);
    uint y = uv.y * (m_ibuf->y - 1);
    colors[pindex] = pixel_buffer[y * m_ibuf->x + x];
  }
  return ParticleFunctionInputArray(colors.as_ref(), true);
}

Optional<ParticleFunctionInputArray> VertexWeightInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();

  if (dynamic_cast<MeshSurfaceContext *>(action_context)) {
    auto *surface_info = dynamic_cast<MeshSurfaceContext *>(action_context);
    return this->compute_weights(
        interface, surface_info, IndexRange(interface.attributes().size()).as_array_ref());
  }
  else if (dynamic_cast<SourceParticleActionContext *>(action_context)) {
    auto *source_info = dynamic_cast<SourceParticleActionContext *>(action_context);
    auto *surface_info = dynamic_cast<MeshSurfaceContext *>(source_info->source_context());
    if (surface_info != nullptr) {
      return this->compute_weights(interface, surface_info, source_info->source_indices());
    }
  }
  return {};
}

Optional<ParticleFunctionInputArray> VertexWeightInputProvider::compute_weights(
    InputProviderInterface &interface,
    MeshSurfaceContext *surface_info,
    ArrayRef<uint> surface_info_mapping)
{
  Object *object = surface_info->object();
  Mesh *mesh = (Mesh *)object->data;
  MDeformVert *vertex_weights = mesh->dvert;

  int group_index = defgroup_name_index(object, m_group_name.data());
  if (group_index == -1 || vertex_weights == nullptr) {
    return {};
  }

  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
  ArrayRef<float3> barycentric_coords = surface_info->barycentric_coords();

  uint size = interface.attributes().size();
  auto weights = BLI::temporary_allocate_array<float>(size);

  for (uint pindex : interface.pindices()) {
    uint source_index = surface_info_mapping[pindex];
    uint triangle_index = surface_info->looptri_indices()[source_index];
    float3 bary_weights = barycentric_coords[source_index];

    const MLoopTri &triangle = triangles[triangle_index];

    uint vert1 = mesh->mloop[triangle.tri[0]].v;
    uint vert2 = mesh->mloop[triangle.tri[1]].v;
    uint vert3 = mesh->mloop[triangle.tri[2]].v;

    float3 corner_weights{defvert_find_weight(vertex_weights + vert1, group_index),
                          defvert_find_weight(vertex_weights + vert2, group_index),
                          defvert_find_weight(vertex_weights + vert3, group_index)};

    float weight = float3::dot(bary_weights, corner_weights);
    weights[pindex] = weight;
  }

  return ParticleFunctionInputArray(weights.as_ref(), true);
}

Optional<ParticleFunctionInputArray> RandomFloatInputProvider::get(
    InputProviderInterface &interface)
{
  ArrayRef<int> ids = interface.attributes().get<int>("ID");

  uint size = interface.attributes().size();
  auto random_values = BLI::temporary_allocate_array<float>(size);

  for (uint pindex : interface.pindices()) {
    float value = BLI_hash_int_01(ids[pindex] + m_seed * 23467);
    random_values[pindex] = value;
  }

  return ParticleFunctionInputArray(random_values.as_ref(), true);
}

Optional<ParticleFunctionInputArray> IsInGroupInputProvider::get(InputProviderInterface &interface)
{
  auto is_in_group_output = BLI::temporary_allocate_array<bool>(interface.attributes().size());

  auto is_in_group_optional = interface.attributes().try_get<bool>(m_group_name);
  if (is_in_group_optional.has_value()) {
    ArrayRef<bool> is_in_group = *is_in_group_optional;
    for (uint pindex : interface.pindices()) {
      is_in_group_output[pindex] = is_in_group[pindex];
    }
  }
  else {
    for (uint pindex : interface.pindices()) {
      is_in_group_output[pindex] = 0;
    }
  }

  return ParticleFunctionInputArray(is_in_group_output.as_ref(), true);
}

}  // namespace BParticles
