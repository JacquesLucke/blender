#pragma once

#include "BKE_falloff.hpp"

#include "actions.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BKE::Falloff;

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

 public:
  TurbulenceForce(std::unique_ptr<ParticleFunction> compute_inputs)
      : m_compute_inputs(std::move(compute_inputs))
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

}  // namespace BParticles
