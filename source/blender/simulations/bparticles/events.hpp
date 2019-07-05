#pragma once

#include "core.hpp"
#include "actions.hpp"

struct BVHTreeFromMesh;

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

EventFilter *EVENT_age_reached(StringRef identifier, SharedFunction &compute_age_fn);
Event *EVENT_mesh_bounce(struct BVHTreeFromMesh *treedata, const float4x4 &transform);

}  // namespace BParticles
