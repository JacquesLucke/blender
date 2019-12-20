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

class CustomForce : public Force {
 private:
  const ParticleFunction &m_inputs_fn;

 public:
  CustomForce(const ParticleFunction &inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void add_force(ForceInterface &interface) override;
};

}  // namespace BParticles
