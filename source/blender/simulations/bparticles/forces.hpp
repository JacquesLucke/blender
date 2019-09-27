#pragma once

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "BKE_bvhutils.h"

#include "DNA_object_types.h"

#include "actions.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BLI::float4x4;

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ForceInterface &interface) = 0;
};

class GravityForce : public Force {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  GravityForce(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void add_force(ForceInterface &interface) override;
};

class TurbulenceForce : public Force {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  TurbulenceForce(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void add_force(ForceInterface &interface) override;
};

class DragForce : public Force {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  DragForce(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void add_force(ForceInterface &interface) override;
};

class MeshForce : public Force {
 private:
  ParticleFunction *m_inputs_fn;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;

 public:
  MeshForce(ParticleFunction *inputs_fn, Object *object) : m_inputs_fn(inputs_fn), m_object(object)
  {
    BLI_assert(object->type == OB_MESH);
    m_local_to_world = m_object->obmat;
    m_world_to_local = m_local_to_world.inverted__LocRotScale();

    BKE_bvhtree_from_mesh_get(&m_bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
  }

  ~MeshForce()
  {
    free_bvhtree_from_mesh(&m_bvhtree_data);
  }

  void add_force(ForceInterface &interface) override;
};

}  // namespace BParticles
