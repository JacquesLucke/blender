#include "emitter.hpp"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace BParticles {

class PointEmitter : public Emitter {
 private:
  float3 m_point;

 public:
  PointEmitter(float3 point) : m_point(point)
  {
  }

  void info(EmitterInfoBuilder &builder) const override
  {
    builder.inits_float3_attribute("Position");
    builder.inits_float3_attribute("Velocity");
  }

  void emit(RequestEmitterBufferCB request_buffers) override
  {
    auto &buffer = request_buffers();
    auto positions = buffer.buffers().get_float3("Position");
    auto velocities = buffer.buffers().get_float3("Velocity");

    positions[0] = m_point;
    velocities[0] = float3{-1, -1, 0};
    buffer.set_initialized(1);
  }
};

std::unique_ptr<Emitter> new_point_emitter(float3 point)
{
  Emitter *emitter = new PointEmitter(point);
  return std::unique_ptr<Emitter>(emitter);
}

}  // namespace BParticles
