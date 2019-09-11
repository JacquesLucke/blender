#pragma once

#include "BKE_falloff.hpp"
#include "BKE_bvhutils.h"

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "DNA_object_types.h"

#include "actions.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BKE::Falloff;
using BLI::float4x4;

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ForceInterface &interface) = 0;
};

class GravityForce : public Force {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Falloff> m_falloff;

 public:
  GravityForce(std::unique_ptr<ParticleFunction> compute_inputs, std::unique_ptr<Falloff> falloff)
      : m_compute_inputs(std::move(compute_inputs)), m_falloff(std::move(falloff))
  {
  }

  void add_force(ForceInterface &interface) override;
};

class TurbulenceForce : public Force {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Falloff> m_falloff;

 public:
  TurbulenceForce(std::unique_ptr<ParticleFunction> compute_inputs,
                  std::unique_ptr<Falloff> falloff)
      : m_compute_inputs(std::move(compute_inputs)), m_falloff(std::move(falloff))
  {
  }

  void add_force(ForceInterface &interface) override;
};

class DragForce : public Force {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Falloff> m_falloff;

 public:
  DragForce(std::unique_ptr<ParticleFunction> compute_inputs, std::unique_ptr<Falloff> falloff)
      : m_compute_inputs(std::move(compute_inputs)), m_falloff(std::move(falloff))
  {
  }

  void add_force(ForceInterface &interface) override;
};

class MeshForce : public Force {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;

 public:
  MeshForce(std::unique_ptr<ParticleFunction> compute_inputs, Object *object)
      : m_compute_inputs(std::move(compute_inputs)), m_object(object)
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
