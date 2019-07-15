#pragma once

#include "FN_tuple_call.hpp"

#include "core.hpp"
#include "world_state.hpp"
#include "action_interface.hpp"

namespace BParticles {

using FN::SharedFunction;
using FN::TupleCallBody;

struct SurfaceEmitter : public Emitter {
  std::string m_particle_type_name;
  std::unique_ptr<Action> m_action;

  Object *m_object;
  InterpolatedFloat4x4 m_transform;
  float m_rate;
  float m_normal_velocity;
  float m_emitter_velocity;
  float m_size;

  void emit(EmitterInterface &interface) override;
};

struct PointEmitter : public Emitter {
  std::string m_particle_type_name;
  InterpolatedFloat3 m_point;
  uint m_amount;

  void emit(EmitterInterface &interface) override;
};

}  // namespace BParticles
