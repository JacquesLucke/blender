#pragma once

#include "simulation_state.hpp"
#include "step_description.hpp"

namespace BParticles {

void simulate_particles(ParticlesState &state,
                        float time_step,
                        ArrayRef<Emitter *> emitters,
                        StringMap<ParticleType *> &types_to_simulate);
};
