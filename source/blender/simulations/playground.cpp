#include "BLI_math.h"
#include "BLI_small_vector.hpp"

#include "SIM_particles.h"

using BLI::SmallVector;

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

struct Vector {
  float x, y, z;
};

class ParticleSystem {
};

class ParticlesState {
 public:
  SmallVector<Vector> m_positions;
};

WRAPPERS(ParticleSystem *, ParticleSystemRef);
WRAPPERS(ParticlesState *, ParticlesStateRef);

ParticleSystemRef SIM_particle_system_new()
{
  return wrap(new ParticleSystem());
}

void SIM_particle_system_free(ParticleSystemRef particle_system)
{
  delete unwrap(particle_system);
}

ParticlesStateRef SIM_particles_state_new(ParticleSystemRef UNUSED(particle_system))
{
  ParticlesState *state = new ParticlesState();
  return wrap(state);
}

void SIM_particles_state_free(ParticlesStateRef state)
{
  delete unwrap(state);
}

void SIM_particle_system_step(ParticlesStateRef state_)
{
  ParticlesState *state = unwrap(state_);
  for (Vector &position : state->m_positions) {
    position.x += 0.1f;
  }
  state->m_positions.append({0, 0, 1});
}

uint SIM_particles_count(ParticlesStateRef state_)
{
  ParticlesState *state = unwrap(state_);
  return state->m_positions.size();
}

void SIM_particles_get_positions(ParticlesStateRef state_, float (*dst)[3])
{
  ParticlesState *state = unwrap(state_);
  memcpy(dst, state->m_positions.begin(), state->m_positions.size() * sizeof(Vector));
}
