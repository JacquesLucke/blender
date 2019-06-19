#pragma once

#include "core.hpp"

struct Mesh;

namespace BParticles {

std::unique_ptr<Emitter> new_point_emitter(float3 point);
std::unique_ptr<Emitter> new_surface_emitter(struct Mesh *mesh);

}  // namespace BParticles
