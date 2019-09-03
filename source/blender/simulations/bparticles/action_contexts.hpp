#pragma once

#include "action_interface.hpp"

struct Object;

namespace BParticles {

using BLI::float4x4;

class MeshSurfaceActionContext : public ActionContext {
 public:
  virtual Object *object() const = 0;
  virtual ArrayRef<float4x4> world_transforms() const = 0;
  virtual ArrayRef<float3> local_positions() const = 0;
  virtual ArrayRef<float3> local_normals() const = 0;
  virtual ArrayRef<float3> world_normals() const = 0;
  virtual ArrayRef<uint> looptri_indices() const = 0;
};

class MeshEmitterContext : public MeshSurfaceActionContext, public EmitterActionContext {
 private:
  Object *m_object;
  ArrayRef<float4x4> m_world_transforms;
  ArrayRef<float3> m_local_positions;
  ArrayRef<float3> m_local_normals;
  ArrayRef<float3> m_world_normals;
  ArrayRef<uint> m_looptri_indices;

  ArrayRef<float4x4> m_all_world_transforms;
  ArrayRef<float3> m_all_local_positions;
  ArrayRef<float3> m_all_local_normals;
  ArrayRef<float3> m_all_world_normals;
  ArrayRef<uint> m_all_looptri_indices;

 public:
  MeshEmitterContext(Object *object,
                     ArrayRef<float4x4> all_world_transforms,
                     ArrayRef<float3> all_local_positions,
                     ArrayRef<float3> all_local_normals,
                     ArrayRef<float3> all_world_normals,
                     ArrayRef<uint> all_looptri_indices)
      : m_object(object),
        m_all_world_transforms(all_world_transforms),
        m_all_local_positions(all_local_positions),
        m_all_local_normals(all_local_normals),
        m_all_world_normals(all_world_normals),
        m_all_looptri_indices(all_looptri_indices)
  {
  }

  void update(Range<uint> slice) override
  {
    m_world_transforms = m_all_world_transforms.slice(slice.start(), slice.size());
    m_local_positions = m_all_local_positions.slice(slice.start(), slice.size());
    m_local_normals = m_all_local_normals.slice(slice.start(), slice.size());
    m_world_normals = m_all_world_normals.slice(slice.start(), slice.size());
    m_looptri_indices = m_all_looptri_indices.slice(slice.start(), slice.size());
  }

  Object *object() const override
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

  Object *object() const override
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
