#pragma once

#include "core.hpp"

namespace BParticles {

Action *ACTION_kill();
Action *ACTION_move(float3 offset);
Action *ACTION_spawn();
Action *ACTION_explode();

}  // namespace BParticles
