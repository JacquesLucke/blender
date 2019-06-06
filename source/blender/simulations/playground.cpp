#include "BLI_math.h"

#include "SIM_particles.h"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

WRAPPERS(void *, ParticleSystemRef);

uint SIM_particles_count(ParticleSystemRef UNUSED(particle_system))
{
  return 5;
}

void SIM_particles_get_positions(ParticleSystemRef UNUSED(particle_system), float (*dst)[3])
{
  for (uint i = 0; i < 5; i++) {
    float vec[3];
    vec[0] = i;
    vec[1] = i * 0.3f;
    vec[2] = 1.0f;
    copy_v3_v3(dst[i], vec);
  }
}
