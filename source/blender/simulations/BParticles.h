
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct Depsgraph;
struct NodeParticlesModifierData;

typedef struct OpaqueBParticlesState *BParticlesState;
typedef struct OpaqueBParticlesWorldState *BParticlesWorldState;

BParticlesState BParticles_new_empty_state(void);
void BParticles_state_free(BParticlesState particles_state);

BParticlesWorldState BParticles_new_world_state(void);
void BParticles_world_state_free(BParticlesWorldState world_state);

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *depsgraph,
                                  BParticlesState particles_state,
                                  BParticlesWorldState world_state);

uint BParticles_state_particle_count(BParticlesState particles_state);
void BParticles_state_get_positions(BParticlesState particles_state, float (*dst)[3]);

struct Mesh *BParticles_test_mesh_from_state(BParticlesState particles_state);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
