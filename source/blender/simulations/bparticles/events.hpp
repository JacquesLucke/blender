#pragma once

#include "core.hpp"

struct BVHTreeFromMesh;

namespace BParticles {

EventFilter *EVENT_age_reached(float age);
Event *EVENT_mesh_bounce(struct BVHTreeFromMesh *treedata, const float4x4 &transform);

}  // namespace BParticles
