
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct NodeParticlesModifierData;

typedef struct OpaqueBParticlesState *BParticlesState;

BParticlesState BParticles_new_empty_state(void);
void BParticles_state_free(BParticlesState state);

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *depsgraph,
                                  BParticlesState state);

uint BParticles_state_particle_count(BParticlesState state);
void BParticles_state_get_positions(BParticlesState state, float (*dst)[3]);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
