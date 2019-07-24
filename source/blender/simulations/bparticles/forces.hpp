#pragma once

#include "step_description.hpp"
#include "actions.hpp"
#include "force_interface.hpp"

namespace BParticles {

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ForceInterface &interface) = 0;
};

class GravityForce : public Force {
 private:
  SharedFunction m_compute_acceleration_fn;
  TupleCallBody *m_compute_acceleration_body;

 public:
  GravityForce(SharedFunction &compute_acceleration_fn)
      : m_compute_acceleration_fn(compute_acceleration_fn)
  {
    m_compute_acceleration_body = m_compute_acceleration_fn->body<TupleCallBody>();
  }

  void add_force(ForceInterface &interface) override;
};

class TurbulenceForce : public Force {
 private:
  ParticleFunction m_compute_inputs;

 public:
  TurbulenceForce(ParticleFunction compute_inputs) : m_compute_inputs(std::move(compute_inputs))
  {
  }

  void add_force(ForceInterface &interface) override;
};

}  // namespace BParticles
