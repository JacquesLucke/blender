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
  MDeformVert *vertices = mesh->dvert;
  int group_index = defgroup_name_index(ob, group_name.data());
  if (group_index == -1 || vertices == nullptr) {
    r_vertex_weights.fill(0);
    return;
  }

  for (uint i = 0; i < mesh->totvert; i++) {
    r_vertex_weights[i] = defvert_find_weight(vertices + i, group_index);
  }
}

static BLI_NOINLINE void get_average_triangle_weights(const Mesh *mesh,
                                                      ArrayRef<MLoopTri> looptris,
                                                      ArrayRef<float> vertex_weights,
                                                      TemporaryArray<float> &r_looptri_weights)
{
  for (uint triangle_index = 0; triangle_index < looptris.size(); triangle_index++) {
    const MLoopTri &looptri = looptris[triangle_index];
    float triangle_weight = 0.0f;
    for (uint i = 0; i < 3; i++) {
      uint vertex_index = mesh->mloop[looptri.tri[i]].v;
      float weight = vertex_weights[vertex_index];
      triangle_weight += weight;
    }

    if (triangle_weight > 0) {
      triangle_weight /= 3.0f;
    }
    r_looptri_weights[triangle_index] = triangle_weight;
  }
}

static BLI_NOINLINE void compute_cumulative_distribution(
    ArrayRef<float> weights, MutableArrayRef<float> r_cumulative_weights)
{
  BLI_assert(weights.size() + 1 == r_cumulative_weights.size());

  r_cumulative_weights[0] = 0;
  for (uint i = 0; i < weights.size(); i++) {
    r_cumulative_weights[i + 1] = r_cumulative_weights[i] + weights[i];
  }
}

static void sample_cumulative_distribution__recursive(uint amount,
                                                      uint start,
                                                      uint one_after_end,
                                                      ArrayRef<float> cumulative_weights,
                                                      VectorAdaptor<uint> &sampled_indices)
{
  BLI_assert(start <= one_after_end);
  uint size = one_after_end - start;
  if (size == 0) {
    BLI_assert(amount == 0);
  }
  else if (amount == 0) {
    return;
  }
  else if (size == 1) {
    sampled_indices.append_n_times(start, amount);
  }
  else {
    uint middle = start + size / 2;
    float left_weight = cumulative_weights[middle] - cumulative_weights[start];
    float right_weight = cumulative_weights[one_after_end] - cumulative_weights[middle];
    BLI_assert(left_weight >= 0.0f && right_weight >= 0.0f);
    float weight_sum = left_weight + right_weight;
    BLI_assert(weight_sum > 0.0f);

    float left_factor = left_weight / weight_sum;
    float right_factor = right_weight / weight_sum;

    uint left_amount = amount * left_factor;
    uint right_amount = amount * right_factor;

    if (left_amount + right_amount < amount) {
      BLI_assert(left_amount + right_amount + 1 == amount);
      float weight_per_item = weight_sum / amount;
      float total_remaining_weight = weight_sum - (left_amount + right_amount) * weight_per_item;
      float left_remaining_weight = left_weight - left_amount * weight_per_item;
      float left_remaining_factor = left_remaining_weight / total_remaining_weight;
      if (random_float() < left_remaining_factor) {
        left_amount++;
      }
      else {
        right_amount++;
      }
    }

    sample_cumulative_distribution__recursive(
        left_amount, start, middle, cumulative_weights, sampled_indices);
    sample_cumulative_distribution__recursive(
        right_amount, middle, one_after_end, cumulative_weights, sampled_indices);
  }
}

static BLI_NOINLINE void sample_cumulative_distribution(uint amount,
                                                        ArrayRef<float> cumulative_weights,
                                                        MutableArrayRef<uint> r_sampled_indices)
{
  BLI_assert(amount == r_sampled_indices.size());

  VectorAdaptor<uint> sampled_indices(r_sampled_indices.begin(), amount);
  sample_cumulative_distribution__recursive(
      amount, 0, cumulative_weights.size() - 1, cumulative_weights, sampled_indices);
  BLI_assert(sampled_indices.is_full());
}

static BLI_NOINLINE bool sample_with_vertex_weights(uint amount,
                                                    Object *ob,
                                                    Mesh *mesh,
                                                    StringRefNull group_name,
                                                    ArrayRef<MLoopTri> triangles,
                                                    MutableArrayRef<uint> r_sampled_looptris)
{
  BLI_assert(amount == r_sampled_looptris.size());

  TemporaryArray<float> vertex_weights(mesh->totvert);
  get_all_vertex_weights(ob, mesh, group_name, vertex_weights);

  TemporaryArray<float> looptri_weights(triangles.size());
  get_average_triangle_weights(mesh, triangles, vertex_weights, looptri_weights);

  TemporaryArray<float> cumulative_looptri_weights(looptri_weights.size() + 1);
  compute_cumulative_distribution(looptri_weights, cumulative_looptri_weights);
  if (ArrayRef<float>(cumulative_looptri_weights).last() == 0.0f) {
    /* All weights are zero. */
    return false;
  }

  sample_cumulative_distribution(amount, cumulative_looptri_weights, r_sampled_looptris);
  return true;
}

static BLI_NOINLINE void sample_randomly(uint amount,
                                         ArrayRef<MLoopTri> triangles,
                                         MutableArrayRef<uint> r_sampled_looptris)
{
  for (uint i = 0; i < amount; i++) {
    r_sampled_looptris[i] = rand() % triangles.size();
  }
}

static BLI_NOINLINE void compute_random_birth_moments(MutableArrayRef<float> r_birth_moments)
{
  for (float &birth_moment : r_birth_moments) {
    birth_moment = random_float();
  }
}

static BLI_NOINLINE void sample_looptris(Mesh *mesh,
                                         ArrayRef<MLoopTri> triangles,
                                         ArrayRef<uint> triangles_to_sample,
                                         MutableArrayRef<float3> r_sampled_positions,
                                         MutableArrayRef<float3> r_sampled_normals)
{
  BLI_assert(triangles_to_sample.size() == r_sampled_positions.size());

  MLoop *loops = mesh->mloop;
  MVert *verts = mesh->mvert;

  for (uint i = 0; i < triangles_to_sample.size(); i++) {
    uint triangle_index = triangles_to_sample[i];
    const MLoopTri &triangle = triangles[triangle_index];

    float3 v1 = verts[loops[triangle.tri[0]].v].co;
    float3 v2 = verts[loops[triangle.tri[1]].v].co;
    float3 v3 = verts[loops[triangle.tri[2]].v].co;

    float3 position = random_point_in_triangle(v1, v2, v3);
    float3 normal;
    normal_tri_v3(normal, v1, v2, v3);

    r_sampled_positions[i] = position;
    r_sampled_normals[i] = normal;
  }
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

  ArrayRef<MLoopTri> triangles(BKE_mesh_runtime_looptri_ensure(mesh),
                               BKE_mesh_runtime_looptri_len(mesh));
  if (triangles.size() == 0) {
    return;
  }

  TemporaryArray<uint> triangles_to_sample(particles_to_emit);
  if (m_density_group.size() > 0) {
    if (!sample_with_vertex_weights(
            particles_to_emit, m_object, mesh, m_density_group, triangles, triangles_to_sample)) {
      return;
    }
  }
  else {
    sample_randomly(particles_to_emit, triangles, triangles_to_sample);
  }

  TemporaryArray<float> birth_moments(particles_to_emit);
  compute_random_birth_moments(birth_moments);

  TemporaryArray<float3> local_positions(particles_to_emit);
  TemporaryArray<float3> local_normals(particles_to_emit);
  sample_looptris(mesh, triangles, triangles_to_sample, local_positions, local_normals);

  float epsilon = 0.01f;
  TemporaryArray<float4x4> transforms_at_birth(particles_to_emit);
  TemporaryArray<float4x4> transforms_before_birth(particles_to_emit);
  m_transform.interpolate(birth_moments, 0.0f, transforms_at_birth);
  m_transform.interpolate(birth_moments, -epsilon, transforms_before_birth);

  TemporaryArray<float> sizes(particles_to_emit);
  sizes.fill(m_size);

  TemporaryArray<float> birth_times(particles_to_emit);
  interface.time_span().interpolate(birth_moments, birth_times);

  Vector<float3> positions;
  Vector<float3> velocities;

  for (uint i = 0; i < particles_to_emit; i++) {
    float3 pos = local_positions[i];
    float3 normal = local_normals[i];

    float4x4 &transform_at_birth = transforms_at_birth[i];
    float4x4 &transform_before_birth = transforms_before_birth[i];

    float3 point_at_birth = transform_at_birth.transform_position(pos);
    float3 point_before_birth = transform_before_birth.transform_position(pos);

    float3 normal_velocity = transform_at_birth.transform_direction(normal);
    float3 emitter_velocity = (point_at_birth - point_before_birth) / epsilon;

    positions.append(point_at_birth);
    velocities.append(normal_velocity * m_normal_velocity + emitter_velocity * m_emitter_velocity);
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
