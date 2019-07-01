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

  void emit(EmitterInterface &interface) override
  {
    auto &target = interface.request(0, 1);
    target.set_float3("Position", {m_point});
    target.set_float3("Velocity", {float3{-1, -1, 0}});
    target.set_birth_moment(1.0f);
  }
};

class SurfaceEmitter : public Emitter {
 private:
  uint m_particle_type_id;
  Mesh *m_mesh;
  float4x4 m_transform_start;
  float4x4 m_transform_end;
  float m_normal_velocity;

 public:
  SurfaceEmitter(uint particle_type_id,
                 Mesh *mesh,
                 float4x4 transform_start,
                 float4x4 transform_end,
                 float normal_velocity)
      : m_particle_type_id(particle_type_id),
        m_mesh(mesh),
        m_transform_start(transform_start),
        m_transform_end(transform_end),
        m_normal_velocity(normal_velocity)
  {
  }

  void emit(EmitterInterface &interface) override
  {
    MLoop *loops = m_mesh->mloop;
    MVert *verts = m_mesh->mvert;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(m_mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(m_mesh);

    SmallVector<float3> positions;
    SmallVector<float3> velocities;
    SmallVector<float> birth_moments;

    for (int i = 0; i < triangle_amount; i++) {
      MLoopTri triangle = triangles[i];
      float birth_moment = (rand() % 1000) / 1000.0f;

      float3 v1 = verts[loops[triangle.tri[0]].v].co;
      float3 v2 = verts[loops[triangle.tri[1]].v].co;
      float3 v3 = verts[loops[triangle.tri[2]].v].co;
      float3 pos = random_point_in_triangle(v1, v2, v3);

      float3 normal;
      normal_tri_v3(normal, v1, v2, v3);

      float4x4 transform = float4x4::interpolate(m_transform_start, m_transform_end, birth_moment);

      positions.append(transform.transform_position(pos));
      velocities.append(transform.transform_direction(normal * m_normal_velocity));
      birth_moments.append(birth_moment);
    }

    auto &target = interface.request(m_particle_type_id, positions.size());
    target.set_float3("Position", positions);
    target.set_float3("Velocity", velocities);
    target.set_birth_moments(birth_moments);
  }

  float3 random_point_in_triangle(float3 a, float3 b, float3 c)
  {
    float3 dir1 = b - a;
    float3 dir2 = c - a;
    float rand1, rand2;

    do {
      rand1 = (rand() % 1000) / 1000.0f;
      rand2 = (rand() % 1000) / 1000.0f;
    } while (rand1 + rand2 > 1.0f);

    return a + dir1 * rand1 + dir2 * rand2;
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

  void emit(EmitterInterface &interface) override
  {
    SmallVector<float3> positions;
    for (uint i = 0; i < m_path.len - 1; i++) {
      float3 pos1 = m_path.data[i].vec;
      float3 pos2 = m_path.data[i + 1].vec;

      for (uint j = 0; j < 10; j++) {
        float factor = (float)j / 100.0f;
        float3 pos = pos1 * (1.0f - factor) + pos2 * factor;
        pos = m_transform.transform_position(pos);
        positions.append(pos);
      }
    }

    auto &target = interface.request(0, positions.size());
    target.set_float3("Position", positions);
    target.set_float3("Velocity", SmallVector<float3>(positions.size()));
    target.set_birth_moment(1.0f);
  }
};

class EmitAtStartEmitter : public Emitter {
  void emit(EmitterInterface &interface) override
  {
    if (!interface.is_first_step()) {
      return;
    }

    SmallVector<float3> positions;
    for (uint i = 0; i < 1000000; i++) {
      positions.append(float3(i / 1000.0f, 0, 0));
    }

    auto &target = interface.request(0, positions.size());
    target.set_float3("Position", positions);
    target.set_birth_moment(0.0f);
  }
};

Emitter *EMITTER_point(float3 point)
{
  return new PointEmitter(point);
}

Emitter *EMITTER_mesh_surface(uint particle_type_id,
                              Mesh *mesh,
                              const float4x4 &transform_start,
                              const float4x4 &transform_end,
                              float normal_velocity)
{
  return new SurfaceEmitter(
      particle_type_id, mesh, transform_start, transform_end, normal_velocity);
}

Emitter *EMITTER_path(Path *path, float4x4 transform)
{
  BLI_assert(path);
  return new PathEmitter(*path, transform);
}

Emitter *EMITTER_emit_at_start()
{
  return new EmitAtStartEmitter();
}

}  // namespace BParticles
