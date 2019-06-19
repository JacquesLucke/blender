#pragma once

#include "core.hpp"

namespace BParticles {

std::unique_ptr<Force> FORCE_directional(float3 force);
std::unique_ptr<Force> FORCE_turbulence(float strength);

}  // namespace BParticles
