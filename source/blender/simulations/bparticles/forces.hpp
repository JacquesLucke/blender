#pragma once

#include "core.hpp"

namespace BParticles {

Force *FORCE_directional(float3 force);
Force *FORCE_turbulence(float strength);

}  // namespace BParticles
