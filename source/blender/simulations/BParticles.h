
#ifndef __SIM_PARTICLES_C_H__
#define __SIM_PARTICLES_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct Depsgraph;
struct BParticlesModifierData;
struct BParticlesFrameCache;
struct Depsgraph;

typedef struct OpaqueBParticlesSimulationState *BParticlesSimulationState;

BParticlesSimulationState BParticles_new_simulation(void);
void BParticles_simulation_free(BParticlesSimulationState simulation_state);

void BParticles_simulate_modifier(struct BParticlesModifierData *bpmd,
                                  struct Depsgraph *depsgraph,
                                  BParticlesSimulationState simulation_state,
                                  float time_step);

Mesh *BParticles_modifier_point_mesh_from_state(BParticlesSimulationState simulation_state);
Mesh *BParticles_modifier_mesh_from_state(BParticlesSimulationState simulation_state);

Mesh *BParticles_state_extract_type__tetrahedons(BParticlesSimulationState simulation_state,
                                                 const char *particle_type);
Mesh *BParticles_state_extract_type__points(BParticlesSimulationState simulation_state,
                                            const char *particle_type);

void BParticles_modifier_free_cache(struct BParticlesModifierData *bpmd);
struct Mesh *BParticles_modifier_mesh_from_cache(struct BParticlesFrameCache *cached_frame);
void BParticles_modifier_cache_state(struct BParticlesModifierData *bpmd,
                                     BParticlesSimulationState simulation_state,
                                     float frame);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_PARTICLES_C_H__ */
