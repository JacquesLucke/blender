
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueParticleSystem *ParticleSystemRef;
typedef struct OpaqueParticlesState *ParticlesStateRef;

ParticleSystemRef SIM_particle_system_new(void);
void SIM_particle_system_free(ParticleSystemRef particle_system);

ParticlesStateRef SIM_particles_state_new(ParticleSystemRef particle_system);
void SIM_particles_state_free(ParticlesStateRef state);
void SIM_particle_system_step(ParticlesStateRef state);

uint SIM_particles_count(ParticlesStateRef state);
void SIM_particles_get_positions(ParticlesStateRef state, float (*dst)[3]);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
