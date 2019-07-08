#pragma once

#include "core.hpp"
#include "actions.hpp"

struct Object;

namespace BParticles {

using BLI::float4x4;

std::unique_ptr<Event> EVENT_mesh_collision(StringRef identifier,
                                            Object *object,
                                            std::unique_ptr<Action> action);
std::unique_ptr<Event> EVENT_age_reached(StringRef identifier,
                                         SharedFunction &compute_age_fn,
                                         std::unique_ptr<Action> action);

}  // namespace BParticles
