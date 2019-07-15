#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh_runtime.h"

#include "BLI_math_geom.h"

#include "emitters.hpp"

namespace BParticles {

static float random_float()
{
  return (rand() % 4096) / 4096.0f;
}

void PointEmitter::emit(EmitterInterface &interface)
{
  SmallVector<float3> new_positions(m_amount);

  for (uint i = 0; i < m_amount; i++) {
    float t = i / (float)m_amount;
    float3 point = float3::interpolate(m_start, m_end, t);
    new_positions[i] = point;
  }

  auto target = interface.particle_allocator().request(m_particle_type_name, new_positions.size());
  target.set_float3("Position", new_positions);
}

static float3 random_point_in_triangle(float3 a, float3 b, float3 c)
{
  float3 dir1 = b - a;
  float3 dir2 = c - a;
  float rand1, rand2;

  do {
    rand1 = random_float();
    rand2 = random_float();
  } while (rand1 + rand2 > 1.0f);

  return a + dir1 * rand1 + dir2 * rand2;
}

void SurfaceEmitter::emit(EmitterInterface &interface)
{
  if (m_object == nullptr) {
    return;
  }
  if (m_object->type != OB_MESH) {
    return;
  }

  float particles_to_emit_f = m_rate * interface.time_span().duration();
  float fraction = particles_to_emit_f - std::floor(particles_to_emit_f);
  if ((rand() % 1000) / 1000.0f < fraction) {
    particles_to_emit_f = std::floor(particles_to_emit_f) + 1;
  }
  uint particles_to_emit = particles_to_emit_f;

  Mesh *mesh = (Mesh *)m_object->data;

  MLoop *loops = mesh->mloop;
  MVert *verts = mesh->mvert;
  const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
  int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);
  if (triangle_amount == 0) {
    return;
  }

  SmallVector<float3> positions;
  SmallVector<float3> velocities;
  SmallVector<float> sizes;
  SmallVector<float> birth_times;

  for (uint i = 0; i < particles_to_emit; i++) {
    MLoopTri triangle = triangles[rand() % triangle_amount];
    float birth_moment = random_float();

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
    velocities.append(normal_velocity + emitter_velocity);
    birth_times.append(interface.time_span().interpolate(birth_moment));
    sizes.append(0.1f);
  }

  auto target = interface.particle_allocator().request(m_particle_type_name, positions.size());
  target.set_float3("Position", positions);
  target.set_float3("Velocity", velocities);
  target.set_float("Size", sizes);
  target.set_float("Birth Time", birth_times);

  ActionInterface::RunFromEmitter(m_action, target, interface);
}

}  // namespace BParticles
