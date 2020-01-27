#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void CustomForce::add_force(ForceInterface &interface)
{
  MutableArrayRef<float3> dst = interface.combined_destination();

  ParticleFunctionEvaluator inputs{m_inputs_fn, interface.mask(), interface.attributes()};
  inputs.context_builder().set_buffer_cache(interface.buffer_cache());
  inputs.compute();

  for (uint pindex : interface.mask()) {
    dst[pindex] += inputs.get_single<float3>("Force", 0, pindex);
  }
}

}  // namespace BParticles
