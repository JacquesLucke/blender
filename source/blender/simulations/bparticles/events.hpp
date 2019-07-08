#pragma once

#include "core.hpp"
#include "actions.hpp"

struct Object;

namespace BParticles {

using BLI::float4x4;

Event *EVENT_mesh_collision(StringRef identifier, Object *object, Action *action);
Event *EVENT_age_reached(StringRef identifier, SharedFunction &compute_age_fn, Action *action);

}  // namespace BParticles
