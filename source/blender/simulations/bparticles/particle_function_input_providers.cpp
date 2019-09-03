#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"
#include "BKE_mesh_runtime.h"

#include "particle_function_input_providers.hpp"
#include "action_contexts.hpp"

namespace BParticles {

ParticleFunctionInputArray AttributeInputProvider::get(InputProviderInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  uint attribute_index = attributes.attribute_index(m_name);
  uint stride = attributes.attribute_stride(attribute_index);
  void *buffer = attributes.get_ptr(attribute_index);
  return {buffer, stride, false};
}

ParticleFunctionInputArray CollisionNormalInputProvider::get(InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  BLI_assert(action_context != nullptr);
  auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
  BLI_assert(surface_info != nullptr);
  return {surface_info->world_normals(), false};
}

ParticleFunctionInputArray AgeInputProvider::get(InputProviderInterface &interface)
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
  return {ArrayRef<float>(ages), true};
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

ParticleFunctionInputArray SurfaceImageInputProvider::get(InputProviderInterface &interface)
{
  ActionContext *action_context = interface.action_context();
  BLI_assert(action_context != nullptr);
  auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
  BLI_assert(surface_info != nullptr);

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
  return {ArrayRef<rgba_f>(colors), true};
}

}  // namespace BParticles
