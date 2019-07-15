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
  float4x4 m_transform_start, m_transform_end;
  float m_rate;

  void emit(EmitterInterface &interface) override;
};

struct PointEmitter : public Emitter {
  std::string m_particle_type_name;
  float3 m_start, m_end;
  uint m_amount;

  void emit(EmitterInterface &interface) override;
};

}  // namespace BParticles
