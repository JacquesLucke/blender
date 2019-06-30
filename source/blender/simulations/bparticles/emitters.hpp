#pragma once

#include "core.hpp"

struct Mesh;
struct Path;

namespace BParticles {

Emitter *EMITTER_point(float3 point);
Emitter *EMITTER_mesh_surface(uint particle_type_id,
                              struct Mesh *mesh,
                              const float4x4 &transform,
                              float normal_velocity);
Emitter *EMITTER_path(struct Path *path, float4x4 transform);
Emitter *EMITTER_emit_at_start();

}  // namespace BParticles
