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
  ParticlesState *state = new ParticlesState();

  AttributesInfo info{{"Kill State"}, {"Birth Time"}, {"Position", "Velocity"}};
  ParticlesContainer *container = new ParticlesContainer(info, 1000);
  state->m_container = container;

  return wrap(state);
}

void BParticles_state_free(BParticlesState state)
{
  delete unwrap(state);
}

class ModifierStepDescription : public StepDescription {
 public:
  SmallVector<Emitter *> m_emitters;
  SmallVector<Force *> m_forces;
  SmallVector<Event *> m_events;
  SmallVector<Action *> m_actions;

  ArrayRef<Emitter *> emitters() override
  {
    return m_emitters;
  }
  ArrayRef<Force *> forces() override
  {
    return m_forces;
  }
  ArrayRef<Event *> events() override
  {
    return m_events;
  }
  ArrayRef<Action *> actions_per_event() override
  {
    return m_actions;
  }
};

void BParticles_simulate_modifier(NodeParticlesModifierData *UNUSED(npmd),
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);
  ModifierStepDescription description;
  description.m_emitters.append(EMITTER_point({1, 1, 1}).release());
  description.m_forces.append(FORCE_directional({0, 0, -2}).release());
  simulate_step(state, description);
}

uint BParticles_state_particle_count(BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);

  uint count = 0;
  for (auto *block : state.m_container->active_blocks()) {
    count += block->active_amount();
  }

  return count;
}

void BParticles_state_get_positions(BParticlesState state_c, float (*dst)[3])
{
  ParticlesState &state = *unwrap(state_c);

  uint index = 0;
  for (auto *block : state.m_container->active_blocks()) {
    auto positions = block->slice_active().get_float3("Position");
    memcpy(dst + index, positions.begin(), sizeof(float3) * positions.size());
    index += positions.size();
  }
}
