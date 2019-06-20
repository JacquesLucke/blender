#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "emitters.hpp"
#include "forces.hpp"
#include "simulate.hpp"

#include "BLI_timeit.hpp"
#include "BLI_listbase.h"

#include "BKE_curve.h"

#include "DNA_object_types.h"
#include "DNA_curve_types.h"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

using namespace BParticles;

using BLI::ArrayRef;
using BLI::float3;
using BLI::SmallVector;
using BLI::StringRef;

WRAPPERS(ParticlesState *, BParticlesState);

/* New Functions
 *********************************************************/

BParticlesState BParticles_new_empty_state()
{
  return wrap(new BParticles::ParticlesState());
}

void BParticles_state_free(BParticlesState state)
{
  delete unwrap(state);
}

class ModifierStepDescription : public StepDescription {
  ArrayRef<Emitter *> emitters()
  {
    return {};
  }
  ArrayRef<Force *> forces()
  {
    return {};
  }
  ArrayRef<Event *> events()
  {
    return {};
  }
  ArrayRef<Action *> actions_per_event()
  {
    return {};
  }
};

void BParticles_simulate_modifier(NodeParticlesModifierData *UNUSED(npmd),
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);
  ModifierStepDescription description;
  simulate_step(state, description);
}

uint BParticles_state_particle_count(BParticlesState UNUSED(state))
{
  return 1;
}

void BParticles_state_get_positions(BParticlesState UNUSED(state), float (*dst)[3])
{
  dst[0][0] = 1;
  dst[0][1] = 2;
  dst[0][2] = 3;
}
