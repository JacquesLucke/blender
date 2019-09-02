#pragma once

#include "block_step_data.hpp"

namespace BParticles {

/**
 * Interface between the Integrator->integrate() function and the core simulation code.
 */
class IntegratorInterface : public BlockStepDataAccess {
 private:
  ArrayRef<uint> m_pindices;

 public:
  IntegratorInterface(BlockStepData &step_data, ArrayRef<uint> pindices)
      : BlockStepDataAccess(step_data), m_pindices(pindices)
  {
  }

  ArrayRef<uint> pindices()
  {
    return m_pindices;
  }
};

/**
 * The integrator is the core of the particle system. It's main task is to determine how the
 * simulation would go if there were no events.
 */
class Integrator {
 public:
  virtual ~Integrator()
  {
  }

  /**
   * Specify which attributes are integrated (usually Position and Velocity).
   */
  virtual AttributesInfo &offset_attributes_info() = 0;

  /**
   * Compute the offsets for all integrated attributes. Those are not applied immediately, because
   * there might be events that modify the attributes within a time step.
   */
  virtual void integrate(IntegratorInterface &interface) = 0;
};

}  // namespace BParticles
