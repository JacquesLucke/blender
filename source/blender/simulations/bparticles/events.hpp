#pragma once

#include "core.hpp"
#include "actions.hpp"

struct Object;

namespace BParticles {

using BLI::float4x4;

class CustomEvent : public Event {
 public:
  virtual ~CustomEvent();

  virtual void attributes(TypeAttributeInterface &UNUSED(interface))
  {
  }
};

Event *EVENT_mesh_collision(StringRef identifier, Object *object, Action *action);
CustomEvent *EVENT_age_reached(StringRef identifier,
                               SharedFunction &compute_age_fn,
                               Action *action);

}  // namespace BParticles
