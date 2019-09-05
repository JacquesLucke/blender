#pragma once

#include "BLI_temporary_allocator.hpp"

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
  ArrayRef<float3> m_world_surface_velocities;
  ArrayRef<float3> m_barycentric_coords;

 public:
  MeshSurfaceContext(Object *object,
                     ArrayRef<float4x4> world_transforms,
                     ArrayRef<float3> local_positions,
                     ArrayRef<float3> local_normals,
                     ArrayRef<float3> world_normals,
                     ArrayRef<uint> looptri_indices,
                     ArrayRef<float3> world_surface_velocities)
      : m_object(object),
        m_world_transforms(world_transforms),
        m_local_positions(local_positions),
        m_local_normals(local_normals),
        m_world_normals(world_normals),
        m_looptri_indices(looptri_indices),
        m_world_surface_velocities(world_surface_velocities)
  {
    this->compute_barycentric_coords(IndexRange(m_local_positions.size()).as_array_ref());
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
    auto world_transforms = BLI::temporary_allocate_array<float4x4>(size);
    auto world_normals = BLI::temporary_allocate_array<float3>(size);
    auto surface_velocities = BLI::temporary_allocate_array<float3>(size);

    for (uint pindex : pindices) {
      world_transforms[pindex] = world_transform;
      world_normals[pindex] = world_transform.transform_direction(local_normals[pindex]);
      surface_velocities[pindex] = float3(0, 0, 0);
    }

    m_world_transforms = world_transforms;
    m_world_normals = world_normals;
    m_world_surface_velocities = surface_velocities;

    m_buffers_to_free.extend(
        {world_transforms.begin(), world_normals.begin(), surface_velocities.begin()});

    this->compute_barycentric_coords(pindices);
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

  ArrayRef<float3> world_surface_velicities() const
  {
    return m_world_surface_velocities;
  }

  ArrayRef<float3> barycentric_coords() const
  {
    return m_barycentric_coords;
  }

 private:
  void compute_barycentric_coords(ArrayRef<uint> pindices);
};

}  // namespace BParticles
