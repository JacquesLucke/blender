#pragma once

#include "step_description.hpp"
#include "actions.hpp"

#include "BKE_bvhutils.h"

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "DNA_object_types.h"

struct Object;

namespace BParticles {

using BLI::float4x4;

class AgeReachedEvent : public Event {
 private:
  std::string m_identifier;
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Action> m_action;

 public:
  AgeReachedEvent(StringRef identifier,
                  std::unique_ptr<ParticleFunction> compute_inputs,
                  std::unique_ptr<Action> action)
      : m_identifier(identifier.to_std_string()),
        m_compute_inputs(std::move(compute_inputs)),
        m_action(std::move(action))
  {
  }

  void attributes(AttributesDeclaration &builder) override;
  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

class CollisionEventInfo : public ActionContext {
 private:
  ArrayRef<float3> m_normals;

 public:
  CollisionEventInfo(ArrayRef<float3> normals) : m_normals(normals)
  {
  }

  ArrayRef<float3> normals()
  {
    return m_normals;
  }
};

class MeshCollisionEvent : public Event {
 private:
  std::string m_identifier;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;
  std::unique_ptr<Action> m_action;

  struct RayCastResult {
    bool success;
    int index;
    float3 normal;
    float distance;
  };

  struct EventStorage {
    float3 normal;
  };

 public:
  MeshCollisionEvent(StringRef identifier, Object *object, std::unique_ptr<Action> action)
      : m_identifier(identifier.to_std_string()), m_object(object), m_action(std::move(action))
  {
    BLI_assert(object->type == OB_MESH);
    m_local_to_world = m_object->obmat;
    m_world_to_local = m_local_to_world.inverted__LocRotScale();

    BKE_bvhtree_from_mesh_get(&m_bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
  }

  ~MeshCollisionEvent()
  {
    free_bvhtree_from_mesh(&m_bvhtree_data);
  }

  void attributes(AttributesDeclaration &builder) override;
  uint storage_size() override;
  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;

 private:
  RayCastResult ray_cast(float3 start, float3 normalized_direction, float max_distance);
};

class CloseByPointsEvent : public Event {
 private:
  std::string m_identifier;
  KDTree_3d *m_kdtree;
  float m_distance;
  std::unique_ptr<Action> m_action;

 public:
  CloseByPointsEvent(StringRef identifier,
                     KDTree_3d *kdtree,
                     float distance,
                     std::unique_ptr<Action> action)
      : m_identifier(identifier.to_std_string()),
        m_kdtree(kdtree),
        m_distance(distance),
        m_action(std::move(action))
  {
  }

  ~CloseByPointsEvent()
  {
    BLI_kdtree_3d_free(m_kdtree);
  }

  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

}  // namespace BParticles
