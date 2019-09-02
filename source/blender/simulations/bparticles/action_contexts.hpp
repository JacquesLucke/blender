#pragma once

#include "action_interface.hpp"

struct Object;

namespace BParticles {

class MeshSurfaceActionContext : public ActionContext {
 public:
  virtual const Object *object() const = 0;
  virtual ArrayRef<float4x4> world_transforms() const = 0;
  virtual ArrayRef<float3> local_positions() const = 0;
  virtual ArrayRef<float3> local_normals() const = 0;
  virtual ArrayRef<float3> world_normals() const = 0;
  virtual ArrayRef<uint> looptri_indices() const = 0;
};

class MeshCollisionContext : public MeshSurfaceActionContext {
 private:
  Object *m_object;
  ArrayRef<float4x4> m_world_transforms;
  ArrayRef<float3> m_local_positions;
  ArrayRef<float3> m_local_normals;
  ArrayRef<float3> m_world_normals;
  ArrayRef<uint> m_looptri_indices;

 public:
  MeshCollisionContext(Object *object,
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

  const Object *object() const override
  {
    return m_object;
  }

  ArrayRef<float4x4> world_transforms() const override
  {
    return m_world_transforms;
  }

  ArrayRef<float3> local_positions() const override
  {
    return m_local_positions;
  }

  ArrayRef<float3> local_normals() const override
  {
    return m_local_normals;
  }

  ArrayRef<float3> world_normals() const override
  {
    return m_world_normals;
  }

  ArrayRef<uint> looptri_indices() const override
  {
    return m_looptri_indices;
  }
};

};  // namespace BParticles
