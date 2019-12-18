#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void CustomForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> dst = interface.combined_destination();

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());

  for (uint pindex : interface.pindices()) {
    dst[pindex] += inputs.get_single<float3>("Force", 0, pindex);
  }
}

}  // namespace BParticles
