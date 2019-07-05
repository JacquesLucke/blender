#pragma once

#include "core.hpp"

namespace BParticles {

class Action {
 public:
  virtual ~Action() = 0;

  virtual void execute(EventExecuteInterface &interface) = 0;
};

Action *ACTION_none();
Action *ACTION_kill();
Action *ACTION_move(float3 offset);
Action *ACTION_spawn();
Action *ACTION_explode(StringRef new_particle_name);

}  // namespace BParticles
