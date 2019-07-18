#pragma once

#include "core.hpp"
#include "actions.hpp"

namespace BParticles {

class Force {
 public:
  virtual ~Force() = 0;
  virtual void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) = 0;
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

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override;
};

class TurbulenceForce : public Force {
 private:
  SharedFunction m_compute_strength_fn;
  TupleCallBody *m_compute_strength_body;

 public:
  TurbulenceForce(SharedFunction &compute_strength_fn) : m_compute_strength_fn(compute_strength_fn)
  {
    m_compute_strength_body = m_compute_strength_fn->body<TupleCallBody>();
  }

  void add_force(ParticlesBlock &block, ArrayRef<float3> r_force) override;
};

class TrailListener : public ForwardingListener {
 private:
  std::string m_particle_type_name;

 public:
  TrailListener(StringRef particle_type_name)
      : m_particle_type_name(particle_type_name.to_std_string())
  {
  }

  void listen(ForwardingListenerInterface &interface) override;
};

}  // namespace BParticles
