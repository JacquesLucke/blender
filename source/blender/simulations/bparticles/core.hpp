#pragma once

#include <memory>
#include <functional>

#include "BLI_array_ref.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_math.hpp"
#include "BLI_utildefines.h"
#include "BLI_string_ref.hpp"

#include "attributes.hpp"
#include "particles_container.hpp"

namespace BParticles {
class Description;
class Solver;
class WrappedState;
class StateBase;
class Emitter;
class EmitterInfo;
class EmitterInfoBuilder;

using BLI::ArrayRef;
using BLI::float3;
using BLI::float4x4;
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

class Action {
 public:
  virtual ~Action();

  virtual void execute(AttributeArrays attributes, ArrayRef<uint> indices_mask) = 0;
};

class PositionalEvent {
 public:
  virtual ~PositionalEvent();

  virtual void filter(AttributeArrays attributes,
                      ArrayRef<uint> indices_mask,
                      ArrayRef<float3> next_movement,
                      SmallVector<uint> &r_filtered_indices,
                      SmallVector<float> &r_time_factors) = 0;
};

class EmitterInfo {
 private:
  EmitterInfo()
  {
  }

  Emitter *m_emitter;
  SmallSetVector<std::string> m_used_float_attributes;
  SmallSetVector<std::string> m_used_float3_attributes;
  SmallSetVector<std::string> m_used_byte_attributes;

  friend EmitterInfoBuilder;

 public:
  Emitter &emitter()
  {
    return *m_emitter;
  }

  ArrayRef<std::string> used_float_attributes()
  {
    return m_used_float_attributes.values();
  }

  ArrayRef<std::string> used_float3_attributes()
  {
    return m_used_float3_attributes.values();
  }

  ArrayRef<std::string> used_byte_attributes()
  {
    return m_used_byte_attributes.values();
  }

  bool uses_float_attribute(StringRef name)
  {
    return m_used_float_attributes.contains(name.to_std_string());
  }

  bool uses_float3_attribute(StringRef name)
  {
    return m_used_float3_attributes.contains(name.to_std_string());
  }

  bool uses_byte_attribute(StringRef name)
  {
    return m_used_byte_attributes.contains(name.to_std_string());
  }
};

class EmitterInfoBuilder {
 private:
  Emitter *m_emitter;
  SmallSetVector<std::string> m_used_byte_attributes;
  SmallSetVector<std::string> m_used_float_attributes;
  SmallSetVector<std::string> m_used_float3_attributes;

 public:
  EmitterInfoBuilder(Emitter *emitter) : m_emitter(emitter)
  {
  }

  void inits_attribute(StringRef name, AttributeType type)
  {
    switch (type) {
      case AttributeType::Byte:
        m_used_byte_attributes.add(name.to_std_string());
        break;
      case AttributeType::Float:
        m_used_float_attributes.add(name.to_std_string());
        break;
      case AttributeType::Float3:
        m_used_float3_attributes.add(name.to_std_string());
        break;
      default:
        BLI_assert(false);
    }
  }

  EmitterInfo build()
  {
    EmitterInfo info;
    info.m_emitter = m_emitter;
    info.m_used_byte_attributes = m_used_byte_attributes;
    info.m_used_float_attributes = m_used_float_attributes;
    info.m_used_float3_attributes = m_used_float3_attributes;
    return info;
  }
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

  virtual void info(EmitterInfoBuilder &info) const = 0;
  virtual void emit(EmitterHelper helper) = 0;
};

class Description {
 private:
  SmallVector<Force *> m_forces;
  SmallVector<Emitter *> m_emitters;

 public:
  Description(ArrayRef<Force *> forces, ArrayRef<Emitter *> emitters)
      : m_forces(forces.to_small_vector()), m_emitters(emitters.to_small_vector())
  {
  }

  ArrayRef<Force *> forces()
  {
    return m_forces;
  }

  ArrayRef<Emitter *> emitters()
  {
    return m_emitters;
  }

  virtual ~Description();
};

class Solver {
 public:
  virtual ~Solver();

  virtual StateBase *init() = 0;
  virtual void step(WrappedState &wrapped_state, float elapsed_seconds) = 0;

  virtual uint particle_amount(WrappedState &wrapped_state) = 0;
  virtual void get_positions(WrappedState &wrapped_state, float (*dst)[3]) = 0;
};

class StateBase {
 public:
  virtual ~StateBase();
};

class WrappedState final {
 private:
  Solver *m_solver;
  std::unique_ptr<StateBase> m_state;

 public:
  WrappedState(Solver *solver, std::unique_ptr<StateBase> state)
      : m_solver(solver), m_state(std::move(state))
  {
    BLI_assert(solver);
    BLI_assert(m_state.get() != NULL);
  }

  WrappedState(WrappedState &other) = delete;
  WrappedState(WrappedState &&other) = delete;
  WrappedState &operator=(WrappedState &other) = delete;
  WrappedState &operator=(WrappedState &&other) = delete;

  Solver &solver() const
  {
    BLI_assert(m_solver);
    return *m_solver;
  }

  template<typename T> T &state() const
  {
    T *state = dynamic_cast<T *>(m_state.get());
    BLI_assert(state);
    return *state;
  }

  friend void adapt_state(Solver *, WrappedState *);
};

void adapt_state(Solver *new_solver, WrappedState *wrapped_state);

}  // namespace BParticles
