#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"
#include "world_state.hpp"
#include "action_interface.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

class SurfaceEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  std::unique_ptr<Action> m_action;

  Object *m_object;
  InterpolatedFloat4x4 m_transform;
  float m_rate;
  float m_normal_velocity;
  float m_emitter_velocity;
  float m_size;

 public:
  SurfaceEmitter(StringRef particle_type_name,
                 std::unique_ptr<Action> action,
                 Object *object,
                 InterpolatedFloat4x4 transform,
                 float rate,
                 float normal_velocity,
                 float emitter_velocity,
                 float size)
      : m_particle_type_name(particle_type_name.to_std_string()),
        m_action(std::move(action)),
        m_object(object),
        m_transform(transform),
        m_rate(rate),
        m_normal_velocity(normal_velocity),
        m_emitter_velocity(emitter_velocity),
        m_size(size)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class PointEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  uint m_amount;
  InterpolatedFloat3 m_point;
  InterpolatedFloat3 m_velocity;
  InterpolatedFloat m_size;

 public:
  PointEmitter(StringRef particle_type_name,
               uint amount,
               InterpolatedFloat3 point,
               InterpolatedFloat3 velocity,
               InterpolatedFloat size)
      : m_particle_type_name(particle_type_name.to_std_string()),
        m_amount(amount),
        m_point(point),
        m_velocity(velocity),
        m_size(size)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class CustomFunctionEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  SharedFunction m_function;

 public:
  CustomFunctionEmitter(StringRef particle_type_name, SharedFunction &function)
      : m_particle_type_name(particle_type_name.to_std_string()), m_function(function)
  {
  }

  void emit(EmitterInterface &interface) override;
};

class InitialGridEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  uint m_amount_x;
  uint m_amount_y;
  float m_step_x;
  float m_step_y;
  float m_size;

 public:
  InitialGridEmitter(StringRef particle_type_name,
                     uint amount_x,
                     uint amount_y,
                     float step_x,
                     float step_y,
                     float size)
      : m_particle_type_name(particle_type_name.to_std_string()),
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
