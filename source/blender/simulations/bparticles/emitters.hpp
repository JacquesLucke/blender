#pragma once

#include "core.hpp"

struct Mesh;

namespace BParticles {

std::unique_ptr<Emitter> EMITTER_point(float3 point);
std::unique_ptr<Emitter> EMITTER_mesh_surface(struct Mesh *mesh, float normal_velocity);

}  // namespace BParticles
