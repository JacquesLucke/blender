#pragma once

#include "step_description_interfaces.hpp"

namespace BParticles {

class ForceInterface : public BlockStepDataAccess {
 private:
  ArrayRef<float3> m_destination;

 public:
  ForceInterface(BlockStepData &step_data, ArrayRef<float3> destination)
      : BlockStepDataAccess(step_data), m_destination(destination)
  {
  }

  ArrayRef<float3> combined_destination()
  {
    return m_destination;
  }
};

}  // namespace BParticles
