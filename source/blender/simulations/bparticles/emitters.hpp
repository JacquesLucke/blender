#pragma once

#include "emitter_interface.hpp"
#include "particle_action.hpp"
#include "world_state.hpp"

#include "FN_multi_function.h"

#include "BKE_id_data_cache.h"
#include "BKE_id_handle.h"

namespace BParticles {

using FN::MultiFunction;

class SurfaceEmitter : public Emitter {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  ParticleAction &m_on_birth_action;

  Object *m_object;
  VaryingFloat4x4 m_transform;
  float m_rate;

  Vector<float> m_vertex_weights;

 public:
  SurfaceEmitter(ArrayRef<std::string> systems_to_emit,
                 ParticleAction &on_birth_action,
                 Object *object,
                 VaryingFloat4x4 transform,
                 float rate,
                 Vector<float> vertex_weights)
      : m_systems_to_emit(systems_to_emit),
        m_on_birth_action(on_birth_action),
        m_object(object),
        m_transform(transform),
        m_rate(rate),
        m_vertex_weights(std::move(vertex_weights))
  {
  }

  void emit(EmitterInterface &interface) override;
};

class PointEmitter : public Emitter {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  VaryingFloat3 m_position;
  VaryingFloat3 m_velocity;
  VaryingFloat m_size;
  ParticleAction &m_action;

 public:
  PointEmitter(ArrayRef<std::string> systems_to_emit,
               VaryingFloat3 position,
               VaryingFloat3 velocity,
               VaryingFloat size,
               ParticleAction &action)
      : m_systems_to_emit(systems_to_emit),
        m_position(position),
        m_velocity(velocity),
        m_size(size),
        m_action(action)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class InitialGridEmitter : public Emitter {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  uint m_amount_x;
  uint m_amount_y;
  float m_step_x;
  float m_step_y;
  float m_size;
  ParticleAction &m_action;

 public:
  InitialGridEmitter(ArrayRef<std::string> systems_to_emit,
                     uint amount_x,
                     uint amount_y,
                     float step_x,
                     float step_y,
                     float size,
                     ParticleAction &action)
      : m_systems_to_emit(systems_to_emit),
        m_amount_x(amount_x),
        m_amount_y(amount_y),
        m_step_x(step_x),
        m_step_y(step_y),
        m_size(size),
        m_action(action)
  {
  }

  void emit(EmitterInterface &interface) override;
};

namespace BirthTimeModes {
enum Enum {
  None = 0,
  Begin = 1,
  End = 2,
  Random = 3,
  Linear = 4,
};
}

class CustomEmitter : public Emitter {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  const MultiFunction &m_emitter_function;
  Vector<std::string> m_attribute_names;
  ParticleAction &m_action;
  BirthTimeModes::Enum m_birth_time_mode;
  const BKE::IDHandleLookup &m_id_handle_lookup;
  const BKE::IDDataCache &m_id_data_cache;

 public:
  CustomEmitter(ArrayRef<std::string> systems_to_emit,
                const MultiFunction &emitter_function,
                Vector<std::string> attribute_names,
                ParticleAction &action,
                BirthTimeModes::Enum birth_time_mode,
                const BKE::IDHandleLookup &id_handle_lookup,
                const BKE::IDDataCache &id_data_cache)
      : m_systems_to_emit(systems_to_emit),
        m_emitter_function(emitter_function),
        m_attribute_names(std::move(attribute_names)),
        m_action(action),
        m_birth_time_mode(birth_time_mode),
        m_id_handle_lookup(id_handle_lookup),
        m_id_data_cache(id_data_cache)
  {
  }

  void emit(EmitterInterface &interface) override;
};

}  // namespace BParticles
