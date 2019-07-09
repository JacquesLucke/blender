#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"
#include "world_state.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

Emitter *EMITTER_point(StringRef particle_type_name, float3 point);

Emitter *EMITTER_mesh_surface(StringRef particle_type_name,
                              SharedFunction &compute_inputs_fn,
                              WorldState &world_state);

}  // namespace BParticles
