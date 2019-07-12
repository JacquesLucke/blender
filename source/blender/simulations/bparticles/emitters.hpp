#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"
#include "world_state.hpp"
#include "action_interface.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

std::unique_ptr<Emitter> EMITTER_point(StringRef particle_type_name,
                                       SharedFunction &compute_inputs);

std::unique_ptr<Emitter> EMITTER_mesh_surface(StringRef particle_type_name,
                                              SharedFunction &compute_inputs_fn,
                                              WorldState &world_state,
                                              std::unique_ptr<Action> action);

}  // namespace BParticles
