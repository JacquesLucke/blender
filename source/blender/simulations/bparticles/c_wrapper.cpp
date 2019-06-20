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

class AgeReachedEvent : public Event {
 private:
  float m_age;

 public:
  AgeReachedEvent(float age) : m_age(age)
  {
  }

  void filter(AttributeArrays attributes,
              ArrayRef<uint> particle_indices,
              IdealOffsets &UNUSED(ideal_offsets),
              ArrayRef<float> durations,
              float end_time,
              SmallVector<uint> &r_filtered_indices,
              SmallVector<float> &r_time_factors) override
  {
    auto birth_times = attributes.get_float("Birth Time");

    for (uint i = 0; i < particle_indices.size(); i++) {
      uint pindex = particle_indices[i];
      float duration = durations[i];
      float birth_time = birth_times[pindex];
      float age = end_time - birth_time;
      if (age >= m_age && age - duration < m_age) {
        r_filtered_indices.append(i);
        float time_factor =
            TimeSpan(end_time - duration, duration).get_factor(birth_time + m_age) + 0.00001f;
        r_time_factors.append(time_factor);
      }
    }
  }
};

class KillAction : public Action {
  void execute(AttributeArrays attributes, ArrayRef<uint> particle_indices) override
  {
    auto kill_states = attributes.get_byte("Kill State");
    for (uint pindex : particle_indices) {
      kill_states[pindex] = 1;
    }
  }
};

class MoveAction : public BParticles::Action {
 private:
  float3 m_offset;

 public:
  MoveAction(float3 offset) : m_offset(offset)
  {
  }

  void execute(AttributeArrays attributes, ArrayRef<uint> particle_indices) override
  {
    auto positions = attributes.get_float3("Position");

    for (uint pindex : particle_indices) {
      positions[pindex] += m_offset;
    }
  }
};

class ModifierStepParticleInfluences : public ParticleInfluences {
 public:
  SmallVector<Force *> m_forces;
  SmallVector<Event *> m_events;
  SmallVector<Action *> m_actions;

  ~ModifierStepParticleInfluences()
  {
    for (Force *force : m_forces) {
      delete force;
    }
    for (Event *event : m_events) {
      delete event;
    }
    for (Action *action : m_actions) {
      delete action;
    }
  }

  ArrayRef<Force *> forces() override
  {
    return m_forces;
  }
  ArrayRef<Event *> events() override
  {
    return m_events;
  }
  ArrayRef<Action *> action_per_event() override
  {
    return m_actions;
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  SmallVector<Emitter *> m_emitters;
  ModifierStepParticleInfluences m_influences;

  ~ModifierStepDescription()
  {
    for (Emitter *emitter : m_emitters) {
      delete emitter;
    }
  }

  float step_duration() override
  {
    return m_duration;
  }

  ArrayRef<Emitter *> emitters() override
  {
    return m_emitters;
  }

  ParticleInfluences &influences() override
  {
    return m_influences;
  }
};

void BParticles_simulate_modifier(NodeParticlesModifierData *UNUSED(npmd),
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);
  ModifierStepDescription description;
  description.m_duration = 1.0f / 24.0f;
  description.m_emitters.append(EMITTER_point({1, 1, 1}).release());
  description.m_influences.m_forces.append(FORCE_directional({0, 0, -2}).release());
  description.m_influences.m_events.append(new AgeReachedEvent(1));
  description.m_influences.m_actions.append(new KillAction());
  description.m_influences.m_events.append(new AgeReachedEvent(0.5f));
  description.m_influences.m_actions.append(new MoveAction({0, 1, 0}));
  simulate_step(state, description);

  std::cout << "Active Blocks: " << state.m_container->active_blocks().size() << "\n";
  std::cout << " Particle Amount: " << BParticles_state_particle_count(state_c) << "\n";
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
