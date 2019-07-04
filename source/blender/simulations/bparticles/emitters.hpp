#pragma once

#include "core.hpp"

struct Mesh;
struct Path;

namespace BParticles {

using BLI::float4x4;

Emitter *EMITTER_point(float3 point);
Emitter *EMITTER_mesh_surface(StringRef particle_type_name,
                              struct Mesh *mesh,
                              const float4x4 &transform_start,
                              const float4x4 &transform_end,
                              float normal_velocity);
Emitter *EMITTER_path(struct Path *path, float4x4 transform);
Emitter *EMITTER_emit_at_start();

}  // namespace BParticles
