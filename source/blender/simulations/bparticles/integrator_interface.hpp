#pragma once

#include "BLI_index_mask.h"

#include "block_step_data.hpp"

namespace BParticles {

using BLI::IndexMask;

/**
 * Interface between the Integrator->integrate() function and the core simulation code.
 */
class IntegratorInterface : public BlockStepDataAccess {
 private:
  IndexMask m_mask;

 public:
  IntegratorInterface(BlockStepData &step_data, IndexMask mask)
      : BlockStepDataAccess(step_data), m_mask(mask)
  {
  }

  IndexMask mask()
  {
    return m_mask;
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
  virtual const AttributesInfo &offset_attributes_info() = 0;

  /**
   * Compute the offsets for all integrated attributes. Those are not applied immediately, because
   * there might be events that modify the attributes within a time step.
   */
  virtual void integrate(IntegratorInterface &interface) = 0;
};

}  // namespace BParticles
