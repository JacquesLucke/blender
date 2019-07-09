#pragma once

#include "core.hpp"
#include "FN_tuple_call.hpp"

struct Mesh;
struct Path;

namespace BParticles {

using BLI::float4x4;
using FN::SharedFunction;
using FN::TupleCallBody;

Emitter *EMITTER_point(StringRef particle_type_name, float3 point);

Emitter *EMITTER_mesh_surface(StringRef particle_type_name, SharedFunction &compute_inputs_fn);

}  // namespace BParticles
