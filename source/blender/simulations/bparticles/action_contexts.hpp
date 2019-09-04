#pragma once

#include "action_interface.hpp"

struct Object;

namespace BParticles {

using BLI::float3;
using BLI::float4x4;

class MeshSurfaceContext : public ActionContext {
 private:
  Vector<void *> m_buffers_to_free;
  Object *m_object;
  ArrayRef<float4x4> m_world_transforms;
  ArrayRef<float3> m_local_positions;
  ArrayRef<float3> m_local_normals;
  ArrayRef<float3> m_world_normals;
  ArrayRef<uint> m_looptri_indices;
  ArrayRef<float3> m_surface_velocities;

 public:
  MeshSurfaceContext(Object *object,
                     ArrayRef<float4x4> world_transforms,
                     ArrayRef<float3> local_positions,
                     ArrayRef<float3> local_normals,
                     ArrayRef<float3> world_normals,
                     ArrayRef<uint> looptri_indices,
                     ArrayRef<float3> surface_velocities)
      : m_object(object),
        m_world_transforms(world_transforms),
        m_local_positions(local_positions),
        m_local_normals(local_normals),
        m_world_normals(world_normals),
        m_looptri_indices(looptri_indices),
        m_surface_velocities(surface_velocities)
  {
  }

  MeshSurfaceContext(Object *object,
                     float4x4 world_transform,
                     ArrayRef<uint> pindices,
                     ArrayRef<float3> local_positions,
                     ArrayRef<float3> local_normals,
                     ArrayRef<uint> looptri_indices)
      : m_object(object),
        m_local_positions(local_positions),
        m_local_normals(local_normals),
        m_looptri_indices(looptri_indices)
  {
    uint size = local_positions.size();
    float4x4 *world_transforms_buffer = (float4x4 *)BLI_temporary_allocate(sizeof(float4x4) *
                                                                           size);
    float3 *world_normals_buffer = (float3 *)BLI_temporary_allocate(sizeof(float3) * size);
    float3 *surface_velocities_buffer = (float3 *)BLI_temporary_allocate(sizeof(float3) * size);

    for (uint pindex : pindices) {
      world_transforms_buffer[pindex] = world_transform;
      world_normals_buffer[pindex] = local_normals[pindex];
      surface_velocities_buffer[pindex] = float3(0, 0, 0);
    }

    m_world_transforms = ArrayRef<float4x4>(world_transforms_buffer, size);
    m_world_normals = ArrayRef<float3>(world_normals_buffer, size);
    m_surface_velocities = ArrayRef<float3>(surface_velocities_buffer, size);

    m_buffers_to_free.extend(
        {world_transforms_buffer, world_normals_buffer, surface_velocities_buffer});
  }

  ~MeshSurfaceContext()
  {
    for (void *buffer : m_buffers_to_free) {
      BLI_temporary_deallocate(buffer);
    }
  }

  MeshSurfaceContext(const MeshSurfaceContext &other) = delete;
  MeshSurfaceContext &operator=(const MeshSurfaceContext &other) = delete;

  Object *object() const
  {
    return m_object;
  }

  ArrayRef<float4x4> world_transforms() const
  {
    return m_world_transforms;
  }

  ArrayRef<float3> local_positions() const
  {
    return m_local_positions;
  }

  ArrayRef<float3> local_normals() const
  {
    return m_local_normals;
  }

  ArrayRef<float3> world_normals() const
  {
    return m_world_normals;
  }

  ArrayRef<uint> looptri_indices() const
  {
    return m_looptri_indices;
  }

  ArrayRef<float3> surface_velicities() const
  {
    return m_surface_velocities;
  }
};

};  // namespace BParticles
