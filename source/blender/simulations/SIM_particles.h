
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueParticleSystem *ParticleSystemRef;

uint SIM_particles_count(ParticleSystemRef particle_system);
void SIM_particles_get_positions(ParticleSystemRef particle_system, float (*dst)[3]);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
