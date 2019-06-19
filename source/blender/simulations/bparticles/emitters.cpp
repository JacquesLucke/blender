#include "emitters.hpp"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"

#include "BKE_curve.h"
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
    builder.inits_attribute("Position", AttributeType::Float3);
    builder.inits_attribute("Velocity", AttributeType::Float3);
  }

  void emit(EmitterHelper helper) override
  {
    auto &target = helper.request_raw();
    auto positions = target.attributes().get_float3("Position");
    auto velocities = target.attributes().get_float3("Velocity");

    positions[0] = m_point;
    velocities[0] = float3{-1, -1, 0};
    target.set_initialized(1);
  }
};

class SurfaceEmitter : public Emitter {
 private:
  Mesh *m_mesh;
  float m_normal_velocity;

 public:
  SurfaceEmitter(Mesh *mesh, float normal_velocity)
      : m_mesh(mesh), m_normal_velocity(normal_velocity)
  {
  }

  void info(EmitterInfoBuilder &builder) const override
  {
    builder.inits_attribute("Position", AttributeType::Float3);
    builder.inits_attribute("Velocity", AttributeType::Float3);
  }

  void emit(EmitterHelper helper) override
  {
    MLoop *loops = m_mesh->mloop;
    MVert *verts = m_mesh->mvert;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(m_mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(m_mesh);

    SmallVector<float3> positions;
    SmallVector<float3> velocities;

    for (int i = 0; i < triangle_amount; i++) {
      MLoopTri triangle = triangles[i];

      float3 v1 = verts[loops[triangle.tri[0]].v].co;
      float3 v2 = verts[loops[triangle.tri[1]].v].co;
      float3 v3 = verts[loops[triangle.tri[2]].v].co;

      float3 normal;
      normal_tri_v3(normal, v1, v2, v3);

      float3 pos = (v1 + v2 + v3) / 3.0f;
      positions.append(pos);
      velocities.append(normal * m_normal_velocity);
    }

    auto target = helper.request(positions.size());
    target.set_float3("Position", positions);
    target.set_float3("Velocity", velocities);
  }
};

class PathEmitter : public Emitter {
 private:
  Path &m_path;
  float4x4 m_transform;

 public:
  PathEmitter(Path &path, float4x4 transform) : m_path(path), m_transform(transform)
  {
  }

  void info(EmitterInfoBuilder &builder) const override
  {
    builder.inits_attribute("Position", AttributeType::Float3);
    builder.inits_attribute("Velocity", AttributeType::Float3);
  }

  void emit(EmitterHelper helper) override
  {
    SmallVector<float3> positions;
    for (uint i = 0; i < m_path.len - 1; i++) {
      float3 pos1 = m_path.data[i].vec;
      float3 pos2 = m_path.data[i + 1].vec;

      for (uint j = 0; j < 100; j++) {
        float factor = (float)j / 100.0f;
        float3 pos = pos1 * (1.0f - factor) + pos2 * factor;
        pos = m_transform.transform_position(pos);
        positions.append(pos);
      }
    }

    auto target = helper.request(positions.size());
    target.set_float3("Position", positions);
    target.set_float3("Velocity", SmallVector<float3>(positions.size()));
  }
};

std::unique_ptr<Emitter> EMITTER_point(float3 point)
{
  Emitter *emitter = new PointEmitter(point);
  return std::unique_ptr<Emitter>(emitter);
}

std::unique_ptr<Emitter> EMITTER_mesh_surface(Mesh *mesh, float normal_velocity)
{
  Emitter *emitter = new SurfaceEmitter(mesh, normal_velocity);
  return std::unique_ptr<Emitter>(emitter);
}

std::unique_ptr<Emitter> EMITTER_path(Path *path, float4x4 transform)
{
  BLI_assert(path);
  Emitter *emitter = new PathEmitter(*path, transform);
  return std::unique_ptr<Emitter>(emitter);
}

}  // namespace BParticles
