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

namespace BParticles {

using BLI::ArrayRef;
using BLI::float3;
using BLI::float4x4;
using BLI::SmallMap;
using BLI::SmallSetVector;
using BLI::SmallVector;
using BLI::StringRef;
using std::unique_ptr;

class Force {
 public:
  virtual ~Force();
  virtual void add_force(AttributeArrays attributes,
                         ArrayRef<uint> indices_mask,
                         ArrayRef<float3> dst) = 0;
};

class Event {
 public:
  virtual ~Event();

  virtual void filter(AttributeArrays attributes,
                      ArrayRef<uint> indices_mask,
                      ArrayRef<float3> next_movement,
                      SmallVector<uint> &r_filtered_indices,
                      SmallVector<float> &r_time_factors) = 0;
};

class Action {
 public:
  virtual ~Action();

  virtual void execute(AttributeArrays attributes, ArrayRef<uint> indices_mask) = 0;
};

class EmitterTarget {
 private:
  AttributeArrays m_attributes;
  uint m_emitted_amount = 0;

 public:
  EmitterTarget(AttributeArrays attributes) : m_attributes(attributes)
  {
  }

  void set_initialized(uint n)
  {
    m_emitted_amount += n;
    BLI_assert(m_emitted_amount <= m_attributes.size());
  }

  uint emitted_amount()
  {
    return m_emitted_amount;
  }

  AttributeArrays &attributes()
  {
    return m_attributes;
  }

  uint size()
  {
    return m_attributes.size();
  }
};

using RequestEmitterTarget = std::function<EmitterTarget &()>;

class EmitterHelper {
 private:
  RequestEmitterTarget &m_request_target;

 public:
  EmitterHelper(RequestEmitterTarget &request_target) : m_request_target(request_target)
  {
  }

  EmitterTarget &request_raw()
  {
    EmitterTarget &target = m_request_target();
    BLI_assert(target.size() > 0);
    return target;
  }

  JoinedAttributeArrays request(uint size)
  {
    SmallVector<AttributeArrays> arrays_list;
    uint remaining_size = size;
    while (remaining_size > 0) {
      EmitterTarget &target = this->request_raw();

      uint size_to_use = std::min(target.size(), remaining_size);
      target.set_initialized(size_to_use);
      arrays_list.append(target.attributes().take_front(size_to_use));
      remaining_size -= size_to_use;
    }

    AttributesInfo &info = (arrays_list.size() == 0) ? this->request_raw().attributes().info() :
                                                       arrays_list[0].info();

    return JoinedAttributeArrays(info, arrays_list);
  }
};

class Emitter {
 public:
  virtual ~Emitter();

  virtual void emit(EmitterHelper helper) = 0;
};

class ParticleInfluences {
 public:
  virtual ArrayRef<Force *> forces() = 0;
  virtual ArrayRef<Event *> events() = 0;
  virtual ArrayRef<Action *> action_per_event() = 0;
};

class StepDescription {
 public:
  virtual float step_duration() = 0;
  virtual ArrayRef<Emitter *> emitters() = 0;
  virtual ParticleInfluences &influences() = 0;
};

class ParticlesState {
 public:
  ParticlesContainer *m_container;
  float m_current_time = 0.0f;

  ParticlesState() = default;
};

}  // namespace BParticles
