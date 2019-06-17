#include "emitter.hpp"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_mesh_runtime.h"
#include "BLI_math_geom.h"

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

class SurfaceEmitter : public Emitter {
 private:
  Mesh *m_mesh;

 public:
  SurfaceEmitter(Mesh *mesh) : m_mesh(mesh)
  {
  }

  void info(EmitterInfoBuilder &builder) const override
  {
    builder.inits_float3_attribute("Position");
    builder.inits_float3_attribute("Velocity");
  }

  void emit(RequestEmitterBufferCB request_buffers) override
  {
    MLoop *loops = m_mesh->mloop;
    MVert *verts = m_mesh->mvert;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(m_mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(m_mesh);

    for (int i = 0; i < triangle_amount; i++) {
      auto &buffer = request_buffers();
      auto positions = buffer.buffers().get_float3("Position");
      auto velocities = buffer.buffers().get_float3("Velocity");

      MLoopTri triangle = triangles[i];

      float3 v1 = verts[loops[triangle.tri[0]].v].co;
      float3 v2 = verts[loops[triangle.tri[1]].v].co;
      float3 v3 = verts[loops[triangle.tri[2]].v].co;

      float3 normal;
      normal_tri_v3(normal, v1, v2, v3);

      float3 pos = (v1 + v2 + v3) / 3.0f;
      positions[0] = pos;
      velocities[0] = normal;
      buffer.set_initialized(1);
    }
  }
};

std::unique_ptr<Emitter> new_point_emitter(float3 point)
{
  Emitter *emitter = new PointEmitter(point);
  return std::unique_ptr<Emitter>(emitter);
}

std::unique_ptr<Emitter> new_surface_emitter(Mesh *mesh)
{
  Emitter *emitter = new SurfaceEmitter(mesh);
  return std::unique_ptr<Emitter>(emitter);
}

}  // namespace BParticles
