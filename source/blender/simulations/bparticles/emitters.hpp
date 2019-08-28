#pragma once

#include "FN_tuple_call.hpp"

#include "step_description.hpp"
#include "world_state.hpp"
#include "action_interface.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

class SurfaceEmitter : public Emitter {
 private:
  Vector<std::string> m_types_to_emit;
  std::unique_ptr<Action> m_on_birth_action;

  Object *m_object;
  VaryingFloat4x4 m_transform;
  float m_rate;
  float m_normal_velocity;
  float m_emitter_velocity;
  float m_size;
  std::string m_density_group;

 public:
  SurfaceEmitter(Vector<std::string> types_to_emit,
                 std::unique_ptr<Action> on_birth_action,
                 Object *object,
                 VaryingFloat4x4 transform,
                 float rate,
                 float normal_velocity,
                 float emitter_velocity,
                 float size,
                 StringRef density_group)
      : m_types_to_emit(std::move(types_to_emit)),
        m_on_birth_action(std::move(on_birth_action)),
        m_object(object),
        m_transform(transform),
        m_rate(rate),
        m_normal_velocity(normal_velocity),
        m_emitter_velocity(emitter_velocity),
        m_size(size),
        m_density_group(density_group)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class PointEmitter : public Emitter {
 private:
  Vector<std::string> m_types_to_emit;
  VaryingFloat3 m_position;
  VaryingFloat3 m_velocity;
  VaryingFloat m_size;

 public:
  PointEmitter(Vector<std::string> types_to_emit,
               VaryingFloat3 position,
               VaryingFloat3 velocity,
               VaryingFloat size)
      : m_types_to_emit(std::move(types_to_emit)),
        m_position(position),
        m_velocity(velocity),
        m_size(size)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class InitialGridEmitter : public Emitter {
 private:
  Vector<std::string> m_types_to_emit;
  uint m_amount_x;
  uint m_amount_y;
  float m_step_x;
  float m_step_y;
  float m_size;

 public:
  InitialGridEmitter(Vector<std::string> types_to_emit,
                     uint amount_x,
                     uint amount_y,
                     float step_x,
                     float step_y,
                     float size)
      : m_types_to_emit(std::move(types_to_emit)),
        m_amount_x(amount_x),
        m_amount_y(amount_y),
        m_step_x(step_x),
        m_step_y(step_y),
        m_size(size)
  {
  }

  void emit(EmitterInterface &interface) override;
};

}  // namespace BParticles
