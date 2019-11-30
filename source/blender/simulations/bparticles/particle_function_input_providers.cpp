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

}  // namespace BParticles
