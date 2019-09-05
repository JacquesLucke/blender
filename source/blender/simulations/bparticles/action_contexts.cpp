#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh_runtime.h"

#include "BLI_math_geom.h"

#include "action_contexts.hpp"

namespace BParticles {

void MeshSurfaceContext::compute_barycentric_coords(ArrayRef<uint> pindices)
{
  BLI_assert(m_object->type == OB_MESH);
  uint size = m_local_positions.size();
  auto barycentric_coords = BLI::temporary_allocate_array<float3>(size);

  Mesh *mesh = (Mesh *)m_object->data;
  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

  for (uint pindex : pindices) {
    uint triangle_index = m_looptri_indices[pindex];
    const MLoopTri &triangle = triangles[triangle_index];
    float3 position = m_local_positions[pindex];

    float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
    float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
    float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

    float3 weights;
    interp_weights_tri_v3(weights, v1, v2, v3, position);

    barycentric_coords[pindex] = weights;
  }

  m_barycentric_coords = barycentric_coords;
  m_buffers_to_free.append(barycentric_coords.begin());
}

}  // namespace BParticles
