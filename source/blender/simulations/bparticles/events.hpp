#pragma once

#include "actions.hpp"

#include "BKE_bvhutils.h"

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "DNA_object_types.h"

struct Object;

namespace BParticles {

using BLI::float3;
using BLI::float4x4;

class AgeReachedEvent : public Event {
 private:
  std::string m_identifier;
  ParticleFunction *m_inputs_fn;
  Action &m_action;

 public:
  AgeReachedEvent(StringRef identifier, ParticleFunction *inputs_fn, Action &action)
      : m_identifier(identifier), m_inputs_fn(inputs_fn), m_action(action)
  {
  }

  void attributes(AttributesDeclaration &builder) override;
  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

class CustomEvent : public Event {
 private:
  std::string m_identifier;
  ParticleFunction *m_inputs_fn;
  Action &m_action;

 public:
  CustomEvent(StringRef identifier, ParticleFunction *inputs_fn, Action &action)
      : m_identifier(identifier), m_inputs_fn(inputs_fn), m_action(action)
  {
  }

  void attributes(AttributesDeclaration &builder) override;
  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

class MeshCollisionEvent : public Event {
 private:
  std::string m_identifier;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;
  Action &m_action;

  struct RayCastResult {
    bool success;
    int index;
    float3 normal;
    float distance;
  };

  struct EventStorage {
    uint looptri_index;
    float3 local_normal;
    float3 local_position;
  };

 public:
  MeshCollisionEvent(StringRef identifier, Object *object, Action &action)
      : m_identifier(identifier), m_object(object), m_action(action)
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

}  // namespace BParticles
