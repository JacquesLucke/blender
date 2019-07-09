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
  std::string m_particle_type_name;
  float3 m_point;

 public:
  PointEmitter(StringRef particle_type_name, float3 point)
      : m_particle_type_name(particle_type_name.to_std_string()), m_point(point)
  {
  }

  void emit(EmitterInterface &interface) override
  {
    auto &target = interface.request(m_particle_type_name, 1);
    target.set_float3("Position", {m_point});
    target.set_float3("Velocity", {float3{-1, -1, 0}});
    target.set_birth_moment(1.0f);
  }
};

class SurfaceEmitter : public Emitter {
 private:
  std::string m_particle_type_name;
  Mesh *m_mesh;
  float4x4 m_transform_start;
  float4x4 m_transform_end;
  float m_normal_velocity;

 public:
  SurfaceEmitter(StringRef particle_type_name,
                 Mesh *mesh,
                 float4x4 transform_start,
                 float4x4 transform_end,
                 float normal_velocity)
      : m_particle_type_name(particle_type_name.to_std_string()),
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

      float epsilon = 0.01f;
      /* TODO: interpolate decomposed matrices */
      float4x4 transform_at_birth = float4x4::interpolate(
          m_transform_start, m_transform_end, birth_moment);
      float4x4 transform_before_birth = float4x4::interpolate(
          m_transform_start, m_transform_end, birth_moment - epsilon);

      float3 point_at_birth = transform_at_birth.transform_position(pos);
      float3 point_before_birth = transform_before_birth.transform_position(pos);

      float3 normal_velocity = transform_at_birth.transform_direction(normal);
      float3 emitter_velocity = (point_at_birth - point_before_birth) / epsilon;

      positions.append(point_at_birth);
      velocities.append(normal_velocity * m_normal_velocity + emitter_velocity * 0.3f);
      birth_moments.append(birth_moment);
    }

    auto &target = interface.request(m_particle_type_name, positions.size());
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

Emitter *EMITTER_point(StringRef particle_type_name, float3 point)
{
  return new PointEmitter(particle_type_name, point);
}

Emitter *EMITTER_mesh_surface(StringRef particle_type_name,
                              Mesh *mesh,
                              const float4x4 &transform_start,
                              const float4x4 &transform_end,
                              float normal_velocity)
{
  return new SurfaceEmitter(
      particle_type_name, mesh, transform_start, transform_end, normal_velocity);
}

}  // namespace BParticles
