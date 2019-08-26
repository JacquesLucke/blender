#pragma once

#include "simulation_state.hpp"
#include "step_description.hpp"

namespace BParticles {

struct ParticleTypeInfo {
  AttributesDeclaration *attributes_declaration;

  Integrator *integrator;
  ArrayRef<Event *> events;
  ArrayRef<OffsetHandler *> offset_handlers;
};

void simulate_particles(SimulationState &state,
                        WorldTransition &world_transition,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleTypeInfo> &types_to_simulate);

};  // namespace BParticles
