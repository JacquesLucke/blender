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
  SmallMap<uint, ParticlesContainer *> m_container_by_id;

 public:
  float m_current_time = 0.0f;

  ParticlesState() = default;
  ParticlesState(ParticlesState &other) = delete;
  ~ParticlesState();

  SmallMap<uint, ParticlesContainer *> &particle_containers()
  {
    return m_container_by_id;
  }

  ParticlesContainer &particle_container(uint type_id)
  {
    return *m_container_by_id.lookup(type_id);
  }

  uint particle_container_id(ParticlesContainer &container)
  {
    for (auto item : m_container_by_id.items()) {
      if (item.value == &container) {
        return item.key;
      }
    }
    BLI_assert(false);
    return 0;
  }
};

/**
 * This class allows allocating new blocks from different particle containers.
 * A single instance is not thread safe, but multiple allocator instances can
 * be used by multiple threads at the same time.
 * It might hand out the same block more than once until it is full.
 */
class BlockAllocator {
 private:
  ParticlesState &m_state;
  SmallVector<ParticlesBlock *> m_non_full_cache;
  SmallVector<ParticlesBlock *> m_allocated_blocks;

 public:
  BlockAllocator(ParticlesState &state);
  BlockAllocator(BlockAllocator &other) = delete;

  ParticlesBlock &get_non_full_block(uint particle_type_id);
  void allocate_block_ranges(uint particle_type_id,
                             uint size,
                             SmallVector<ParticlesBlock *> &r_blocks,
                             SmallVector<Range<uint>> &r_ranges);

  AttributesInfo &attributes_info(uint particle_type_id);

  ParticlesState &particles_state()
  {
    return m_state;
  }

  ArrayRef<ParticlesBlock *> allocated_blocks()
  {
    return m_allocated_blocks;
  }
};

class EmitTargetBase {
 protected:
  uint m_particle_type_id;
  AttributesInfo &m_attributes_info;
  SmallVector<ParticlesBlock *> m_blocks;
  SmallVector<Range<uint>> m_ranges;

  uint m_size = 0;

 public:
  EmitTargetBase(uint particle_type_id,
                 AttributesInfo &attributes_info,
                 ArrayRef<ParticlesBlock *> blocks,
                 ArrayRef<Range<uint>> ranges)
      : m_particle_type_id(particle_type_id),
        m_attributes_info(attributes_info),
        m_blocks(blocks),
        m_ranges(ranges)
  {
    BLI_assert(blocks.size() == ranges.size());
    for (auto range : ranges) {
      m_size += range.size();
    }
  }

  EmitTargetBase(EmitTargetBase &other) = delete;

  void set_byte(uint index, ArrayRef<uint8_t> data);
  void set_byte(StringRef name, ArrayRef<uint8_t> data);
  void set_float(uint index, ArrayRef<float> data);
  void set_float(StringRef name, ArrayRef<float> data);
  void set_float3(uint index, ArrayRef<float3> data);
  void set_float3(StringRef name, ArrayRef<float3> data);

  void fill_byte(uint index, uint8_t value);
  void fill_byte(StringRef name, uint8_t value);
  void fill_float(uint index, float value);
  void fill_float(StringRef name, float value);
  void fill_float3(uint index, float3 value);
  void fill_float3(StringRef name, float3 value);

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
  void fill_elements(uint index, void *value);
};

class InstantEmitTarget : public EmitTargetBase {
 public:
  InstantEmitTarget(uint particle_type_id,
                    AttributesInfo &attributes_info,
                    ArrayRef<ParticlesBlock *> blocks,
                    ArrayRef<Range<uint>> ranges)
      : EmitTargetBase(particle_type_id, attributes_info, blocks, ranges)
  {
  }
};

class TimeSpanEmitTarget : public EmitTargetBase {
 private:
  TimeSpan m_time_span;

 public:
  TimeSpanEmitTarget(uint particle_type_id,
                     AttributesInfo &attributes_info,
                     ArrayRef<ParticlesBlock *> blocks,
                     ArrayRef<Range<uint>> ranges,
                     TimeSpan time_span)
      : EmitTargetBase(particle_type_id, attributes_info, blocks, ranges), m_time_span(time_span)
  {
  }

  void set_birth_moment(float time_factor);
  void set_randomized_birth_moments();
};

class EmitterInterface {
 private:
  BlockAllocator &m_block_allocator;
  SmallVector<TimeSpanEmitTarget *> m_targets;
  TimeSpan m_time_span;

 public:
  EmitterInterface(BlockAllocator &allocator, TimeSpan time_span)
      : m_block_allocator(allocator), m_time_span(time_span)
  {
  }

  ~EmitterInterface();

  ArrayRef<TimeSpanEmitTarget *> targets()
  {
    return m_targets;
  }

  TimeSpanEmitTarget &request(uint particle_type_id, uint size);
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
  BlockAllocator &m_block_allocator;
  SmallVector<InstantEmitTarget *> m_emit_targets;
  ArrayRef<float> m_current_times;

 public:
  ActionInterface(ParticleSet particles,
                  BlockAllocator &block_allocator,
                  ArrayRef<float> current_times)
      : m_particles(particles), m_block_allocator(block_allocator), m_current_times(current_times)
  {
  }

  ~ActionInterface();

  BlockAllocator &block_allocator()
  {
    return m_block_allocator;
  }

  ParticleSet &particles()
  {
    return m_particles;
  }

  InstantEmitTarget &request_emit_target(uint particle_type_id, uint size);

  ArrayRef<InstantEmitTarget *> emit_targets()
  {
    return m_emit_targets;
  }

  ArrayRef<float> current_times()
  {
    return m_current_times;
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
