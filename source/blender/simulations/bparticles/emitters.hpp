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

std::unique_ptr<Emitter> EMITTER_mesh_surface(StringRef particle_type_name,
                                              SharedFunction &compute_inputs_fn,
                                              WorldState &world_state,
                                              std::unique_ptr<Action> action);

std::unique_ptr<Emitter> EMITTER_moving_point(StringRef particle_type_name,
                                              float3 start,
                                              float3 end);

}  // namespace BParticles
