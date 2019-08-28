#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"

#include "BLI_math_geom.h"
#include "BLI_vector_adaptor.hpp"

#include "FN_types.hpp"

#include "emitters.hpp"

namespace BParticles {

using BLI::VectorAdaptor;
using FN::SharedList;
using namespace FN::Types;

static float random_float()
{
  return (rand() % 4096) / 4096.0f;
}

void PointEmitter::emit(EmitterInterface &interface)
{
  uint amount = 10;
  Vector<float3> new_positions(amount);
  Vector<float3> new_velocities(amount);
  Vector<float> new_sizes(amount);
  Vector<float> birth_times(amount);

  for (uint i = 0; i < amount; i++) {
    float t = i / (float)amount;
    new_positions[i] = m_position.interpolate(t);
    new_velocities[i] = m_velocity.interpolate(t);
    new_sizes[i] = m_size.interpolate(t);
    birth_times[i] = interface.time_span().interpolate(t);
  }

  for (StringRef type : m_types_to_emit) {
    auto new_particles = interface.particle_allocator().request(type, new_positions.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.set<float3>("Velocity", new_velocities);
    new_particles.set<float>("Size", new_sizes);
    new_particles.set<float>("Birth Time", birth_times);
  }
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

static BLI_NOINLINE void get_all_vertex_weights(Object *ob,
                                                Mesh *mesh,
                                                StringRefNull group_name,
                                                MutableArrayRef<float> r_vertex_weights)
{
  int group_index = defgroup_name_index(ob, group_name.data());
  if (group_index == -1) {
    r_vertex_weights.fill(0);
    return;
  }

  MDeformVert *vertices = mesh->dvert;
  for (uint i = 0; i < mesh->totvert; i++) {
    r_vertex_weights[i] = defvert_find_weight(vertices + i, group_index);
  }
}

static BLI_NOINLINE float get_average_poly_weights(const Mesh *mesh,
                                                   ArrayRef<float> vertex_weights,
                                                   TemporaryVector<float> &r_poly_weights,
                                                   TemporaryVector<uint> &r_polys_with_weight)
{
  float weight_sum = 0.0f;

  for (uint poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
    const MPoly &poly = mesh->mpoly[poly_index];
    float poly_weight = 0.0f;
    for (const MLoop &loop : BLI::ref_c_array(mesh->mloop + poly.loopstart, poly.totloop)) {
      poly_weight += vertex_weights[loop.v];
    }
    if (poly_weight > 0) {
      poly_weight /= poly.totloop;
      r_polys_with_weight.append(poly_index);
      r_poly_weights.append(poly_weight);
      weight_sum += poly_weight;
    }
  }

  return weight_sum;
}

static BLI_NOINLINE void sample_weighted_slots(uint amount,
                                               ArrayRef<float> weights,
                                               float total_weight,
                                               MutableArrayRef<uint> r_sampled_indices)
{
  BLI_assert(amount == r_sampled_indices.size());

  float remaining_weight = total_weight;
  uint remaining_amount = amount;
  VectorAdaptor<uint> all_samples(r_sampled_indices.begin(), amount);

  for (uint i = 0; i < weights.size(); i++) {
    float weight = weights[i];
    float factor = weight / remaining_weight;
    float samples_of_index = factor * remaining_amount;
    float frac = samples_of_index - floorf(samples_of_index);
    if (random_float() < frac) {
      samples_of_index += 1;
    }
    uint int_samples_of_index = (uint)samples_of_index;
    all_samples.append_n_times(i, int_samples_of_index);

    remaining_weight -= weight;
    remaining_amount -= int_samples_of_index;
  }
  BLI_assert(all_samples.is_full());
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

  // TemporaryArray<float> vertex_weights(mesh->totvert);
  // TemporaryVector<float> poly_weights;
  // TemporaryVector<uint> poly_indices_with_weight;
  // get_all_vertex_weights(m_object, mesh, m_density_group, vertex_weights);
  // float weight_sum = get_average_poly_weights(
  //     mesh, vertex_weights, poly_weights, poly_indices_with_weight);

  // TemporaryArray<uint> sampled_weighted_indices(particles_to_emit);
  // sample_weighted_slots(particles_to_emit, poly_weights, weight_sum, sampled_weighted_indices);

  Vector<float3> positions;
  Vector<float3> velocities;
  Vector<float> sizes;
  Vector<float> birth_times;

  for (uint i = 0; i < particles_to_emit; i++) {
    // uint poly_index = poly_indices_with_weight[sampled_weighted_indices[i]];
    // MPoly &poly = mesh->mpoly[poly_index];
    // uint triangle_start_index = poly_to_tri_count(poly_index, poly.loopstart);
    // uint triangle_index = triangle_start_index + (rand() % (poly.totloop - 2));
    // MLoopTri triangle = triangles[triangle_index];
    MLoopTri triangle = triangles[rand() % triangle_amount];
    float birth_moment = random_float();

    float3 v1 = verts[loops[triangle.tri[0]].v].co;
    float3 v2 = verts[loops[triangle.tri[1]].v].co;
    float3 v3 = verts[loops[triangle.tri[2]].v].co;
    float3 pos = random_point_in_triangle(v1, v2, v3);

    float3 normal;
    normal_tri_v3(normal, v1, v2, v3);

    float epsilon = 0.01f;
    float4x4 transform_at_birth = m_transform.interpolate(birth_moment);
    float4x4 transform_before_birth = m_transform.interpolate(birth_moment - epsilon);

    float3 point_at_birth = transform_at_birth.transform_position(pos);
    float3 point_before_birth = transform_before_birth.transform_position(pos);

    float3 normal_velocity = transform_at_birth.transform_direction(normal);
    float3 emitter_velocity = (point_at_birth - point_before_birth) / epsilon;

    positions.append(point_at_birth);
    velocities.append(normal_velocity * m_normal_velocity + emitter_velocity * m_emitter_velocity);
    birth_times.append(interface.time_span().interpolate(birth_moment));
    sizes.append(m_size);
  }

  for (StringRef type_name : m_types_to_emit) {
    auto new_particles = interface.particle_allocator().request(type_name, positions.size());
    new_particles.set<float3>("Position", positions);
    new_particles.set<float3>("Velocity", velocities);
    new_particles.set<float>("Size", sizes);
    new_particles.set<float>("Birth Time", birth_times);

    m_on_birth_action->execute_from_emitter(new_particles, interface);
  }
}

void InitialGridEmitter::emit(EmitterInterface &interface)
{
  if (!interface.is_first_step()) {
    return;
  }

  Vector<float3> new_positions;

  float offset_x = -(m_amount_x * m_step_x / 2.0f);
  float offset_y = -(m_amount_y * m_step_y / 2.0f);

  for (uint x = 0; x < m_amount_x; x++) {
    for (uint y = 0; y < m_amount_y; y++) {
      new_positions.append(float3(x * m_step_x + offset_x, y * m_step_y + offset_y, 0.0f));
    }
  }

  for (StringRef type_name : m_types_to_emit) {
    auto new_particles = interface.particle_allocator().request(type_name, new_positions.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.fill<float>("Birth Time", interface.time_span().start());
    new_particles.fill<float>("Size", m_size);
  }
}

}  // namespace BParticles
