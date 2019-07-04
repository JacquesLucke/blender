#pragma once

#include "core.hpp"

struct Mesh;
struct Path;

namespace BParticles {

using BLI::float4x4;

Emitter *EMITTER_point(StringRef particle_type_name, float3 point);

Emitter *EMITTER_mesh_surface(StringRef particle_type_name,
                              struct Mesh *mesh,
                              const float4x4 &transform_start,
                              const float4x4 &transform_end,
                              float normal_velocity);

}  // namespace BParticles
