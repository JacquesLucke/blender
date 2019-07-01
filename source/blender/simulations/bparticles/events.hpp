#pragma once

#include "core.hpp"

struct BVHTreeFromMesh;

namespace BParticles {

using BLI::float4x4;

class EventFilter {
 public:
  virtual ~EventFilter() = 0;

  virtual void filter(EventFilterInterface &interface) = 0;
};

EventFilter *EVENT_age_reached(float age);
Event *EVENT_mesh_bounce(struct BVHTreeFromMesh *treedata, const float4x4 &transform);

}  // namespace BParticles
