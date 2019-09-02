#pragma once

#include "simulation_state.hpp"
#include "integrator_interface.hpp"
#include "event_interface.hpp"
#include "offset_handler_interface.hpp"
#include "emitter_interface.hpp"

namespace BParticles {

struct ParticleTypeInfo {
  AttributesDeclaration *attributes_declaration;

  Integrator *integrator;
  ArrayRef<Event *> events;
  ArrayRef<OffsetHandler *> offset_handlers;
};

void simulate_particles(SimulationState &state,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleTypeInfo> &types_to_simulate);

}  // namespace BParticles
