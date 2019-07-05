#pragma once

#include "core.hpp"
#include "actions.hpp"

struct Object;

namespace BParticles {

using BLI::float4x4;

class EventFilter {
 public:
  virtual ~EventFilter() = 0;

  virtual void filter(EventFilterInterface &interface) = 0;

  virtual void triggered(EventExecuteInterface &UNUSED(interface))
  {
  }

  virtual void attributes(TypeAttributeInterface &UNUSED(interface))
  {
  }
};

EventFilter *EVENT_mesh_collision(StringRef identifier, Object *object);
EventFilter *EVENT_age_reached(StringRef identifier, SharedFunction &compute_age_fn);

}  // namespace BParticles
