#pragma once

#include "simulation_state.hpp"
#include "integrator_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"
#include "emitter_interface.hpp"
#include "forces.hpp"

#include "DNA_object_types.h"

namespace BParticles {

struct ParticleSystemInfo {
  ArrayRef<Force *> forces;
  ArrayRef<Event *> events;
  ArrayRef<OffsetHandler *> offset_handlers;
  ArrayRef<Object *> collision_objects;
};

void simulate_particles(SimulationState &state,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleSystemInfo> &systems_to_simulate);

}  // namespace BParticles
