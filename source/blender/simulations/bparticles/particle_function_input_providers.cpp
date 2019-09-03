#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"

#include "particle_function_input_providers.hpp"
#include "action_contexts.hpp"

namespace BParticles {

Optional<ParticleFunctionInputArray> AttributeInputProvider::get(InputProviderInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  uint element_size = size_of_attribute_type(m_type);
  int attribute_index = attributes.info().attribute_index_try(m_name, m_type);

  if (attribute_index == -1) {
    return {};
  }
  else {
    void *buffer = attributes.get_ptr(attribute_index);
    return ParticleFunctionInputArray(buffer, element_size, false);
  }
}

Optional<ParticleFunctionInputArray> CollisionNormalInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
  if (surface_info == nullptr) {
    return {};
  }

  return ParticleFunctionInputArray(surface_info->world_normals(), false);
}

Optional<ParticleFunctionInputArray> AgeInputProvider::get(InputProviderInterface &interface)
{
  auto birth_times = interface.attributes().get<float>("Birth Time");
  float *ages_buffer = (float *)BLI_temporary_allocate(sizeof(float) * birth_times.size());
  MutableArrayRef<float> ages(ages_buffer, birth_times.size());

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
  return ParticleFunctionInputArray(ArrayRef<float>(ages), true);
}

SurfaceImageInputProvider::SurfaceImageInputProvider(Image *image) : m_image(image)
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
  auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
  if (surface_info == nullptr) {
    return {};
  }

  const Object *object = surface_info->object();
  Mesh *mesh = (Mesh *)object->data;

  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

  int uv_layer_index = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPUV);
  BLI_assert(uv_layer_index >= 0);
  MLoopUV *uv_layer = (MLoopUV *)CustomData_get(&mesh->ldata, uv_layer_index, CD_MLOOPUV);
  BLI_assert(uv_layer != nullptr);

  ArrayRef<float3> local_positions = surface_info->local_positions();

  rgba_b *pixel_buffer = (rgba_b *)m_ibuf->rect;

  rgba_f *colors_buffer = (rgba_f *)BLI_temporary_allocate(sizeof(rgba_f) *
                                                           local_positions.size());
  MutableArrayRef<rgba_f> colors{colors_buffer, local_positions.size()};

  for (uint pindex : interface.pindices()) {
    float3 local_position = local_positions[pindex];

    uint triangle_index = surface_info->looptri_indices()[pindex];
    const MLoopTri &triangle = triangles[triangle_index];

    uint loop1 = triangle.tri[0];
    uint loop2 = triangle.tri[1];
    uint loop3 = triangle.tri[2];

    float3 v1 = mesh->mvert[mesh->mloop[loop1].v].co;
    float3 v2 = mesh->mvert[mesh->mloop[loop2].v].co;
    float3 v3 = mesh->mvert[mesh->mloop[loop3].v].co;

    float2 uv1 = uv_layer[loop1].uv;
    float2 uv2 = uv_layer[loop2].uv;
    float2 uv3 = uv_layer[loop3].uv;

    float3 vertex_weights;
    interp_weights_tri_v3(vertex_weights, v1, v2, v3, local_position);

    float2 uv;
    interp_v2_v2v2v2(uv, uv1, uv2, uv3, vertex_weights);
    uv = uv.clamped_01();
    uint x = uv.x * (m_ibuf->x - 1);
    uint y = uv.y * (m_ibuf->y - 1);
    colors[pindex] = pixel_buffer[y * m_ibuf->x + x];
  }
  return ParticleFunctionInputArray(ArrayRef<rgba_f>(colors), true);
}

static BLI_NOINLINE Optional<ParticleFunctionInputArray> compute_vertex_weights(
    InputProviderInterface &interface,
    StringRef group_name,
    MeshSurfaceActionContext *surface_info,
    ArrayRef<uint> surface_info_mapping)
{
  Object *object = surface_info->object();
  Mesh *mesh = (Mesh *)object->data;
  MDeformVert *vertex_weights = mesh->dvert;

  int group_index = defgroup_name_index(object, group_name.data());
  if (group_index == -1 || vertex_weights == nullptr) {
    return {};
  }

  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

  float *weight_buffer = (float *)BLI_temporary_allocate(sizeof(float) *
                                                         interface.attributes().size());
  MutableArrayRef<float> weights(weight_buffer, interface.attributes().size());

  for (uint pindex : interface.pindices()) {
    uint source_index = surface_info_mapping[pindex];
    float3 local_position = surface_info->local_positions()[source_index];
    uint triangle_index = surface_info->looptri_indices()[source_index];
    const MLoopTri &triangle = triangles[triangle_index];

    uint loop1 = triangle.tri[0];
    uint loop2 = triangle.tri[1];
    uint loop3 = triangle.tri[2];

    uint vert1 = mesh->mloop[loop1].v;
    uint vert2 = mesh->mloop[loop2].v;
    uint vert3 = mesh->mloop[loop3].v;

    float3 v1 = mesh->mvert[vert1].co;
    float3 v2 = mesh->mvert[vert2].co;
    float3 v3 = mesh->mvert[vert3].co;

    float3 bary_weights;
    interp_weights_tri_v3(bary_weights, v1, v2, v3, local_position);

    float3 corner_weights{defvert_find_weight(vertex_weights + vert1, group_index),
                          defvert_find_weight(vertex_weights + vert2, group_index),
                          defvert_find_weight(vertex_weights + vert3, group_index)};

    float weight = float3::dot(bary_weights, corner_weights);
    weights[pindex] = weight;
  }

  return ParticleFunctionInputArray(ArrayRef<float>(weights), true);
}

Optional<ParticleFunctionInputArray> VertexWeightInputProvider::get(
    InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();

  if (dynamic_cast<MeshSurfaceActionContext *>(action_context)) {
    auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
    return compute_vertex_weights(interface,
                                  m_group_name,
                                  surface_info,
                                  Range<uint>(0, interface.attributes().size()).as_array_ref());
  }
  else if (dynamic_cast<SourceParticleActionContext *>(action_context)) {
    auto *source_info = dynamic_cast<SourceParticleActionContext *>(action_context);
    auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(source_info->source_context());
    return compute_vertex_weights(
        interface, m_group_name, surface_info, source_info->source_indices());
  }
  else {
    return {};
  }
}

}  // namespace BParticles
