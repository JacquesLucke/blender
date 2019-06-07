
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueBParticlesDescription *BParticlesDescription;
typedef struct OpaqueBParticlesSolver *BParticlesSolver;
typedef struct OpaqueBParticlesState *BParticlesState;

BParticlesDescription BParticles_playground_description(void);
void BParticles_description_free(BParticlesDescription description);

BParticlesSolver BParticles_solver_build(BParticlesDescription description);
void BParticles_solver_free(BParticlesSolver solver);

BParticlesState BParticles_state_init(BParticlesSolver solver);
void BParticles_state_adapt(BParticlesSolver new_solver, BParticlesState *state_to_adapt);
void BParticles_state_step(BParticlesSolver solver, BParticlesState state);
void BParticles_state_free(BParticlesState state);

uint BParticles_state_particle_count(BParticlesSolver solver, BParticlesState state);
void BParticles_state_get_positions(BParticlesSolver solver,
                                    BParticlesState state,
                                    float (*dst)[3]);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
