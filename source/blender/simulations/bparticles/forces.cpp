#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void CustomForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> dst = interface.combined_destination();

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    dst[pindex] += inputs->get<float3>("Force", 0, pindex);
  }
}

}  // namespace BParticles
