#pragma once

#include "core.hpp"

struct Mesh;
struct Path;

namespace BParticles {

std::unique_ptr<Emitter> EMITTER_point(float3 point);
std::unique_ptr<Emitter> EMITTER_mesh_surface(uint particle_type_id,
                                              struct Mesh *mesh,
                                              const float4x4 &transform,
                                              float normal_velocity);
std::unique_ptr<Emitter> EMITTER_path(struct Path *path, float4x4 transform);

}  // namespace BParticles
