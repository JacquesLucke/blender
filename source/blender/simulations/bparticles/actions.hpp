#pragma once

#include "core.hpp"

namespace BParticles {

std::unique_ptr<Action> ACTION_kill();
std::unique_ptr<Action> ACTION_move(float3 offset);
std::unique_ptr<Action> ACTION_spawn();

}  // namespace BParticles
