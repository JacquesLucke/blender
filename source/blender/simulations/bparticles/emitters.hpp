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

struct PointEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  InterpolatedFloat3 m_point;
  uint m_amount;

 public:
  PointEmitter(StringRef particle_type_name, InterpolatedFloat3 point, uint amount)
      : m_particle_type_name(particle_type_name.to_std_string()), m_point(point), m_amount(amount)
  {
  }

  void emit(EmitterInterface &interface) override;
};

}  // namespace BParticles
