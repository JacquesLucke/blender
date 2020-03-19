#pragma once

#include "emitter_interface.hpp"
#include "event_interface.hpp"
#include "integrator_interface.hpp"
#include "offset_handler_interface.hpp"
#include "simulation_state.hpp"

namespace BParticles {

struct ParticleSystemInfo {
  Integrator *integrator;
  ArrayRef<Event *> events;
  ArrayRef<OffsetHandler *> offset_handlers;
};

void simulate_particles(SimulationState &state,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleSystemInfo> &systems_to_simulate);

}  // namespace BParticles
