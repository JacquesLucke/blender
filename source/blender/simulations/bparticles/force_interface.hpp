#pragma once

#include "BLI_index_mask.h"

#include "block_step_data.hpp"

namespace BParticles {

using BLI::float3;
using BLI::IndexMask;

class ForceInterface : public BlockStepDataAccess {
 private:
  IndexMask m_mask;
  MutableArrayRef<float3> m_destination;

 public:
  ForceInterface(BlockStepData &step_data, IndexMask mask, MutableArrayRef<float3> destination)
      : BlockStepDataAccess(step_data), m_mask(mask), m_destination(destination)
  {
  }

  IndexMask mask()
  {
    return m_mask;
  }

  MutableArrayRef<float3> combined_destination()
  {
    return m_destination;
  }
};

}  // namespace BParticles
