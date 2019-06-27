#pragma once

#include <memory>
#include <functional>

#include "BLI_array_ref.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_math.hpp"
#include "BLI_utildefines.h"
#include "BLI_string_ref.hpp"
#include "BLI_small_map.hpp"

#include "attributes.hpp"
#include "particles_container.hpp"
#include "time_span.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::float3;
using BLI::float4x4;
using BLI::SmallMap;
using BLI::SmallSetVector;
using BLI::SmallVector;
using BLI::StringRef;
using std::unique_ptr;

class ParticlesState {
 private:
  SmallMap<uint, ParticlesContainer *> m_particle_containers;

 public:
  float m_current_time = 0.0f;

  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  SmallMap<uint, ParticlesContainer *> &particle_containers()
  {
    return m_particle_containers;
  }

  ParticlesContainer &particle_container(uint type_id)
  {
    return *m_particle_containers.lookup(type_id);
  }
};

class BlockAllocator {
 private:
  ParticlesState &m_state;
  SmallVector<ParticlesBlock *> m_block_cache;

 public:
  BlockAllocator(ParticlesState &state);

  ParticlesBlock &get_non_full_block(uint particle_type_id);
};

class EmitTarget {
 private:
  uint m_particle_type_id;
  AttributesInfo &m_attributes_info;
  SmallVector<ParticlesBlock *> m_blocks;
  SmallVector<Range<uint>> m_ranges;
  uint m_size = 0;

 public:
  EmitTarget(uint particle_type_id,
             AttributesInfo &attributes_info,
             ArrayRef<ParticlesBlock *> blocks,
             ArrayRef<Range<uint>> ranges)
      : m_particle_type_id(particle_type_id),
        m_attributes_info(attributes_info),
        m_blocks(blocks.to_small_vector()),
        m_ranges(ranges.to_small_vector())
  {
    BLI_assert(blocks.size() == ranges.size());
    for (auto range : ranges) {
      m_size += range.size();
    }
  }

  void set_byte(uint index, ArrayRef<uint8_t> data);
  void set_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_float(uint index, ArrayRef<float> data);
  void set_float(StringRef name, ArrayRef<float> data);
  void set_float3(uint index, ArrayRef<float3> data);
  void set_float3(StringRef name, ArrayRef<float3> data);

  ArrayRef<ParticlesBlock *> blocks()
  {
    return m_blocks;
  }

  ArrayRef<Range<uint>> ranges()
  {
    return m_ranges;
  }

  uint part_amount()
  {
    return m_ranges.size();
  }

  AttributeArrays attributes(uint part)
  {
    return m_blocks[part]->slice(m_ranges[part]);
  }

  uint particle_type_id()
  {
    return m_particle_type_id;
  }

 private:
  void set_elements(uint index, void *data);
};

class EmitterInterface {
 private:
  ParticlesState &m_state;
  BlockAllocator &m_allocator;
  SmallVector<EmitTarget> m_targets;

 public:
  EmitterInterface(ParticlesState &state, BlockAllocator &allocator)
      : m_state(state), m_allocator(allocator)
  {
  }

  ArrayRef<EmitTarget> targets()
  {
    return m_targets;
  }

  EmitTarget &request(uint particle_type_id, uint size);
};

struct ParticleSet {
 private:
  ParticlesBlock &m_block;

  /* Indices into the attribute arrays.
   * Invariants:
   *   - Every index must exist at most once.
   *   - The indices must be sorted. */
  ArrayRef<uint> m_particle_indices;

 public:
  ParticleSet(ParticlesBlock &block, ArrayRef<uint> particle_indices)
      : m_block(block), m_particle_indices(particle_indices)
  {
  }

  ParticlesBlock &block()
  {
    return m_block;
  }

  AttributeArrays attributes()
  {
    return m_block.slice_all();
  }

  ArrayRef<uint> indices()
  {
    return m_particle_indices;
  }

  uint get_particle_index(uint i)
  {
    return m_particle_indices[i];
  }

  Range<uint> range()
  {
    return Range<uint>(0, m_particle_indices.size());
  }

  uint size()
  {
    return m_particle_indices.size();
  }
};

class Force {
 public:
  virtual ~Force();
  virtual void add_force(ParticleSet particles, ArrayRef<float3> dst) = 0;
};

struct IdealOffsets {
  ArrayRef<float3> position_offsets;
  ArrayRef<float3> velocity_offsets;
};

class EventInterface {
 private:
  ParticleSet m_particles;
  IdealOffsets &m_ideal_offsets;
  ArrayRef<float> m_durations;
  float m_end_time;

  SmallVector<uint> &m_filtered_indices;
  SmallVector<float> &m_filtered_time_factors;

 public:
  EventInterface(ParticleSet particles,
                 IdealOffsets &ideal_offsets,
                 ArrayRef<float> durations,
                 float end_time,
                 SmallVector<uint> &r_filtered_indices,
                 SmallVector<float> &r_filtered_time_factors)
      : m_particles(particles),
        m_ideal_offsets(ideal_offsets),
        m_durations(durations),
        m_end_time(end_time),
        m_filtered_indices(r_filtered_indices),
        m_filtered_time_factors(r_filtered_time_factors)
  {
  }

  ParticleSet &particles()
  {
    return m_particles;
  }

  ArrayRef<float> durations()
  {
    return m_durations;
  }

  TimeSpan time_span(uint index)
  {
    float duration = m_durations[index];
    return TimeSpan(m_end_time - duration, duration);
  }

  IdealOffsets &ideal_offsets()
  {
    return m_ideal_offsets;
  }

  float end_time()
  {
    return m_end_time;
  }

  void trigger_particle(uint index, float time_factor)
  {
    m_filtered_indices.append(index);
    m_filtered_time_factors.append(time_factor);
  }
};

class Event {
 public:
  virtual ~Event();

  virtual void filter(EventInterface &interface) = 0;
};

class ActionInterface {
 private:
  ParticleSet m_particles;

 public:
  ActionInterface(ParticleSet particles) : m_particles(particles)
  {
  }

  ParticleSet &particles()
  {
    return m_particles;
  }
};

class Action {
 public:
  virtual ~Action();

  virtual void execute(ActionInterface &interface) = 0;
};

class Emitter {
 public:
  virtual ~Emitter();

  virtual void emit(EmitterInterface &interface) = 0;
};

class ParticleType {
 public:
  virtual ~ParticleType();

  virtual ArrayRef<Force *> forces() = 0;
  virtual ArrayRef<Event *> events() = 0;
  virtual ArrayRef<Action *> action_per_event() = 0;
};

class StepDescription {
 public:
  virtual ~StepDescription();

  virtual float step_duration() = 0;
  virtual ArrayRef<Emitter *> emitters() = 0;

  virtual ArrayRef<uint> particle_type_ids() = 0;
  virtual ParticleType &particle_type(uint type_id) = 0;
};

}  // namespace BParticles
