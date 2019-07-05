#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(EventExecuteInterface &interface) = 0;
};

Action *ACTION_none();
Action *ACTION_change_direction(SharedFunction &compute_direction_fn);
Action *ACTION_kill();
Action *ACTION_move(float3 offset);
Action *ACTION_spawn();
Action *ACTION_explode(StringRef new_particle_name);

}  // namespace BParticles
