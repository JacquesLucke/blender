#pragma once

#include "action_interface.hpp"

struct Object;

namespace BParticles {

using BLI::float3;
using BLI::float4x4;

class MeshSurfaceContext : public ActionContext {
 private:
  Object *m_object;
  ArrayRef<float4x4> m_world_transforms;
  ArrayRef<float3> m_local_positions;
  ArrayRef<float3> m_local_normals;
  ArrayRef<float3> m_world_normals;
  ArrayRef<uint> m_looptri_indices;

 public:
  MeshSurfaceContext(Object *object,
                     ArrayRef<float4x4> world_transforms,
                     ArrayRef<float3> local_positions,
                     ArrayRef<float3> local_normals,
                     ArrayRef<float3> world_normals,
                     ArrayRef<uint> looptri_indices)
      : m_object(object),
        m_world_transforms(world_transforms),
        m_local_positions(local_positions),
        m_local_normals(local_normals),
        m_world_normals(world_normals),
        m_looptri_indices(looptri_indices)
  {
  }

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
};

};  // namespace BParticles
