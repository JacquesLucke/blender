#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"
#include "BKE_surface_hook.h"

#include "BLI_math_geom.h"
#include "BLI_vector_adaptor.h"

#include "FN_multi_function_common_contexts.h"

#include "emitters.hpp"

namespace BParticles {

using BKE::SurfaceHook;
using BLI::LargeScopedArray;
using BLI::VectorAdaptor;

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

  for (StringRef type : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(type, new_positions.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.set<float3>("Velocity", new_velocities);
    new_particles.set<float>("Size", new_sizes);
    new_particles.set<float>("Birth Time", birth_times);

    m_action.execute_from_emitter(new_particles, interface);
  }
}

static float3 random_uniform_bary_coords()
{
  float rand1 = random_float();
  float rand2 = random_float();

  if (rand1 + rand2 > 1.0f) {
    rand1 = 1.0f - rand1;
    rand2 = 1.0f - rand2;
  }

  return float3(rand1, rand2, 1.0f - rand1 - rand2);
}

static BLI_NOINLINE void get_average_triangle_weights(const Mesh *mesh,
                                                      ArrayRef<MLoopTri> looptris,
                                                      ArrayRef<float> vertex_weights,
                                                      MutableArrayRef<float> r_looptri_weights)
{
  for (uint triangle_index : looptris.index_iterator()) {
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
  for (uint i : weights.index_iterator()) {
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

static BLI_NOINLINE void compute_triangle_areas(Mesh *mesh,
                                                ArrayRef<MLoopTri> triangles,
                                                MutableArrayRef<float> r_areas)
{
  BLI_assert(triangles.size() == r_areas.size());

  for (uint i : triangles.index_iterator()) {
    const MLoopTri &triangle = triangles[i];

    float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
    float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
    float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

    float area = area_tri_v3(v1, v2, v3);
    r_areas[i] = area;
  }
}

static BLI_NOINLINE bool sample_weighted_buckets(uint sample_amount,
                                                 ArrayRef<float> weights,
                                                 MutableArrayRef<uint> r_samples)
{
  BLI_assert(sample_amount == r_samples.size());

  LargeScopedArray<float> cumulative_weights(weights.size() + 1);
  compute_cumulative_distribution(weights, cumulative_weights);

  if (sample_amount > 0 && cumulative_weights.as_ref().last() == 0.0f) {
    /* All weights are zero. */
    return false;
  }

  sample_cumulative_distribution(sample_amount, cumulative_weights, r_samples);
  return true;
}

static BLI_NOINLINE void sample_looptris(Mesh *mesh,
                                         ArrayRef<MLoopTri> triangles,
                                         ArrayRef<uint> triangles_to_sample,
                                         MutableArrayRef<float3> r_sampled_positions,
                                         MutableArrayRef<float3> r_sampled_normals,
                                         MutableArrayRef<float3> r_sampled_bary_coords)
{
  BLI_assert(triangles_to_sample.size() == r_sampled_positions.size());

  MLoop *loops = mesh->mloop;
  MVert *verts = mesh->mvert;

  for (uint i : triangles_to_sample.index_iterator()) {
    uint triangle_index = triangles_to_sample[i];
    const MLoopTri &triangle = triangles[triangle_index];

    float3 v1 = verts[loops[triangle.tri[0]].v].co;
    float3 v2 = verts[loops[triangle.tri[1]].v].co;
    float3 v3 = verts[loops[triangle.tri[2]].v].co;

    float3 bary_coords = random_uniform_bary_coords();

    float3 position;
    interp_v3_v3v3v3(position, v1, v2, v3, bary_coords);

    float3 normal;
    normal_tri_v3(normal, v1, v2, v3);

    r_sampled_positions[i] = position;
    r_sampled_normals[i] = normal;
    r_sampled_bary_coords[i] = bary_coords;
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
  if (m_rate <= 0.0f) {
    return;
  }

  Vector<float> birth_moments;
  float factor_start, factor_step;
  interface.time_span().uniform_sample_range(m_rate, factor_start, factor_step);
  for (float factor = factor_start; factor < 1.0f; factor += factor_step) {
    birth_moments.append(factor);
  }
  std::random_shuffle(birth_moments.begin(), birth_moments.end());

  uint particles_to_emit = birth_moments.size();

  Mesh *mesh = (Mesh *)m_object->data;

  const MLoopTri *triangles_buffer = BKE_mesh_runtime_looptri_ensure(mesh);
  ArrayRef<MLoopTri> triangles(triangles_buffer, BKE_mesh_runtime_looptri_len(mesh));
  if (triangles.size() == 0) {
    return;
  }

  LargeScopedArray<float> triangle_weights(triangles.size());
  get_average_triangle_weights(mesh, triangles, m_vertex_weights, triangle_weights);

  LargeScopedArray<float> triangle_areas(triangles.size());
  compute_triangle_areas(mesh, triangles, triangle_areas);

  for (uint i : triangles.index_iterator()) {
    triangle_weights[i] *= triangle_areas[i];
  }

  LargeScopedArray<uint> triangles_to_sample(particles_to_emit);
  if (!sample_weighted_buckets(particles_to_emit, triangle_weights, triangles_to_sample)) {
    return;
  }

  LargeScopedArray<float3> local_positions(particles_to_emit);
  LargeScopedArray<float3> local_normals(particles_to_emit);
  LargeScopedArray<float3> bary_coords(particles_to_emit);
  sample_looptris(
      mesh, triangles, triangles_to_sample, local_positions, local_normals, bary_coords);

  float epsilon = 0.01f;
  LargeScopedArray<float4x4> transforms_at_birth(particles_to_emit);
  LargeScopedArray<float4x4> transforms_before_birth(particles_to_emit);
  m_transform.interpolate(birth_moments, 0.0f, transforms_at_birth);
  m_transform.interpolate(birth_moments, -epsilon, transforms_before_birth);

  LargeScopedArray<float3> positions_at_birth(particles_to_emit);
  float4x4::transform_positions(transforms_at_birth, local_positions, positions_at_birth);

  LargeScopedArray<float3> surface_velocities(particles_to_emit);
  for (uint i = 0; i < particles_to_emit; i++) {
    float3 position_before_birth = transforms_before_birth[i].transform_position(
        local_positions[i]);
    surface_velocities[i] = (positions_at_birth[i] - position_before_birth) / epsilon /
                            interface.time_span().duration();
  }

  LargeScopedArray<float3> world_normals(particles_to_emit);
  float4x4::transform_directions(transforms_at_birth, local_normals, world_normals);

  LargeScopedArray<float> birth_times(particles_to_emit);
  interface.time_span().interpolate(birth_moments, birth_times);

  LargeScopedArray<SurfaceHook> emit_hooks(particles_to_emit);
  BKE::ObjectIDHandle object_handle(m_object);
  for (uint i = 0; i < particles_to_emit; i++) {
    emit_hooks[i] = SurfaceHook(object_handle, triangles_to_sample[i], bary_coords[i]);
  }

  for (StringRef system_name : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(system_name,
                                                                positions_at_birth.size());
    new_particles.set<float3>("Position", positions_at_birth);
    new_particles.set<float>("Birth Time", birth_times);
    new_particles.set<SurfaceHook>("Emit Hook", emit_hooks);

    m_on_birth_action.execute_from_emitter(new_particles, interface);
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

  for (StringRef system_name : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(system_name, new_positions.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.fill<float>("Birth Time", interface.time_span().start());
    new_particles.fill<float>("Size", m_size);

    m_action.execute_from_emitter(new_particles, interface);
  }
}

using FN::MFDataType;
using FN::MFParamType;

void CustomEmitter::emit(EmitterInterface &interface)
{
  FN::MFParamsBuilder params_builder{m_emitter_function, 1};

  for (uint param_index : m_emitter_function.param_indices()) {
    MFParamType param_type = m_emitter_function.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    BLI_assert(param_type.is_output());
    switch (data_type.category()) {
      case MFDataType::Single: {
        const FN::CPPType &type = data_type.single__cpp_type();
        void *buffer = MEM_mallocN(type.size(), __func__);
        FN::GenericMutableArrayRef array{type, buffer, 1};
        params_builder.add_single_output(array);
        break;
      }
      case MFDataType::Vector: {
        const FN::CPPType &base_type = data_type.vector__cpp_base_type();
        FN::GenericVectorArray *vector_array = new FN::GenericVectorArray(base_type, 1);
        params_builder.add_vector_output(*vector_array);
        break;
      }
    }
  }

  TimeSpan time_span = interface.time_span();
  FN::EmitterTimeInfoContext time_context;
  time_context.begin = time_span.start();
  time_context.end = time_span.end();
  time_context.duration = time_span.duration();
  time_context.step = interface.time_step();

  FN::MFContextBuilder context_builder;
  context_builder.add_global_context(m_id_data_cache);
  context_builder.add_global_context(m_id_handle_lookup);
  context_builder.add_global_context(time_context);

  m_emitter_function.call({0}, params_builder, context_builder);

  int particle_count = -1;

  for (uint param_index : m_emitter_function.param_indices()) {
    MFParamType param_type = m_emitter_function.param_type(param_index);
    if (param_type.is_vector_output()) {
      FN::GenericVectorArray &vector_array = params_builder.computed_vector_array(param_index);
      FN::GenericArrayRef array = vector_array[0];
      particle_count = std::max<int>(particle_count, array.size());
    }
  }

  if (particle_count == -1) {
    particle_count = 1;
  }

  for (StringRef system_name : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(system_name, particle_count);

    switch (m_birth_time_mode) {
      case BirthTimeModes::None:
      case BirthTimeModes::End:
        new_particles.fill<float>("Birth Time", time_span.end());
        break;
      case BirthTimeModes::Begin:
        new_particles.fill<float>("Birth Time", time_span.start());
        break;
      case BirthTimeModes::Linear: {
        LargeScopedArray<float> birth_times(new_particles.total_size());
        time_span.sample_linear(birth_times);
        new_particles.set<float>("Birth Time", birth_times);
        break;
      }
      case BirthTimeModes::Random: {
        LargeScopedArray<float> birth_times(new_particles.total_size());
        for (uint i = 0; i < particle_count; i++) {
          birth_times[i] = time_span.interpolate(random_float());
        }
        new_particles.set<float>("Birth Time", birth_times);
        break;
      }
    }

    const AttributesInfo &info = new_particles.info();

    for (uint param_index : m_emitter_function.param_indices()) {
      MFParamType param_type = m_emitter_function.param_type(param_index);
      StringRef attribute_name = m_attribute_names[param_index];
      if (param_type.is_vector_output()) {
        FN::GenericVectorArray &vector_array = params_builder.computed_vector_array(param_index);
        FN::GenericArrayRef array = vector_array[0];
        const FN::CPPType &base_type = array.type();
        if (info.has_attribute(attribute_name, base_type)) {
          if (array.size() == 0) {
            const void *default_buffer = new_particles.info().default_of(attribute_name);
            new_particles.fill(attribute_name, base_type, default_buffer);
          }
          else {
            new_particles.set_repeated(attribute_name, array);
          }
        }
      }
      else if (param_type.is_single_output()) {
        FN::GenericMutableArrayRef array = params_builder.computed_array(param_index);
        const FN::CPPType &type = array.type();
        if (info.has_attribute(attribute_name, type)) {
          new_particles.fill(attribute_name, type, array[0]);
        }
      }
      else {
        BLI_assert(false);
      }
    }

    m_action.execute_from_emitter(new_particles, interface);
  }

  for (uint param_index : m_emitter_function.param_indices()) {
    MFParamType param_type = m_emitter_function.param_type(param_index);
    if (param_type.is_vector_output()) {
      params_builder.computed_vector_array(param_index).~GenericVectorArray();
    }
    else if (param_type.is_single_output()) {
      FN::GenericMutableArrayRef array = params_builder.computed_array(param_index);
      BLI_assert(array.size() == 1);
      array.destruct_all();
      MEM_freeN(array.buffer());
    }
    else {
      BLI_assert(false);
    }
  }
}

}  // namespace BParticles
