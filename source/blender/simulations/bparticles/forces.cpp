#include "BLI_noise.h"

#include "forces.hpp"

namespace BParticles {

Force::~Force()
{
}

void CustomForce::add_force(AttributesRef attributes,
                            IndexMask mask,
                            BufferCache &buffer_cache,
                            MutableArrayRef<float3> r_destination)
{
  ParticleFunctionEvaluator inputs{m_inputs_fn, mask, attributes};
  inputs.context_builder().set_buffer_cache(buffer_cache);
  inputs.compute();

  for (uint pindex : mask) {
    r_destination[pindex] += inputs.get_single<float3>("Force", 0, pindex);
  }
}

}  // namespace BParticles
