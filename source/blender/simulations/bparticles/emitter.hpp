#pragma once

#include "core.hpp"

namespace BParticles {

std::unique_ptr<Emitter> new_point_emitter(float3 point);
}
