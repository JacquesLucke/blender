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
  std::string m_is_triggered_attribute;
  ParticleFunction *m_inputs_fn;
  ParticleAction &m_action;

 public:
  AgeReachedEvent(StringRef is_triggered_attribute,
                  ParticleFunction *inputs_fn,
                  ParticleAction &action)
      : m_is_triggered_attribute(is_triggered_attribute), m_inputs_fn(inputs_fn), m_action(action)
  {
  }

  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

class CustomEvent : public Event {
 private:
  std::string m_is_triggered_attribute;
  ParticleFunction *m_inputs_fn;
  ParticleAction &m_action;

 public:
  CustomEvent(StringRef is_triggered_attribute,
              ParticleFunction *inputs_fn,
              ParticleAction &action)
      : m_is_triggered_attribute(is_triggered_attribute), m_inputs_fn(inputs_fn), m_action(action)
  {
  }

  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;
};

class MeshCollisionEvent : public Event {
 private:
  std::string m_last_collision_attribute;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world_begin;
  float4x4 m_world_to_local_begin;
  float4x4 m_local_to_world_end;
  float4x4 m_world_to_local_end;
  ParticleAction &m_action;

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
  MeshCollisionEvent(StringRef last_collision_attribute,
                     Object *object,
                     ParticleAction &action,
                     float4x4 local_to_world_begin,
                     float4x4 local_to_world_end)
      : m_last_collision_attribute(last_collision_attribute), m_object(object), m_action(action)
  {
    BLI_assert(object->type == OB_MESH);
    m_local_to_world_begin = local_to_world_begin;
    m_local_to_world_end = local_to_world_end;
    m_world_to_local_begin = m_local_to_world_begin.inverted__LocRotScale();
    m_world_to_local_end = m_local_to_world_end.inverted__LocRotScale();

    BKE_bvhtree_from_mesh_get(&m_bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
  }

  ~MeshCollisionEvent()
  {
    free_bvhtree_from_mesh(&m_bvhtree_data);
  }

  uint storage_size() override;
  void filter(EventFilterInterface &interface) override;
  void execute(EventExecuteInterface &interface) override;

 private:
  RayCastResult ray_cast(float3 start, float3 normalized_direction, float max_distance);
};

}  // namespace BParticles
