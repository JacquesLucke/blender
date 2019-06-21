#pragma once

#include "core.hpp"

struct BVHTreeFromMesh;

namespace BParticles {

std::unique_ptr<Event> EVENT_age_reached(float age);
std::unique_ptr<Event> EVENT_mesh_collection(struct BVHTreeFromMesh *treedata);

}  // namespace BParticles
