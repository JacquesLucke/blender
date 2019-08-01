#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh_runtime.h"

#include "BLI_math_geom.h"

#include "FN_types.hpp"

#include "emitters.hpp"

namespace BParticles {

using FN::Types::SharedFloat3List;
using FN::Types::SharedFloatList;
using FN::Types::SharedFloatRGBAList;
using FN::Types::SharedInt32List;

static float random_float()
{
  return (rand() % 4096) / 4096.0f;
}

void PointEmitter::emit(EmitterInterface &interface)
{
  Vector<float3> new_positions(m_amount);
  Vector<float3> new_velocities(m_amount);
  Vector<float> new_sizes(m_amount);
  Vector<float> birth_times(m_amount);

  for (uint i = 0; i < m_amount; i++) {
    float t = i / (float)m_amount;
    new_positions[i] = m_point.interpolate(t);
    new_velocities[i] = m_velocity.interpolate(t);
    new_sizes[i] = m_size.interpolate(t);
    birth_times[i] = interface.time_span().interpolate(t);
  }

  auto new_particles = interface.particle_allocator().request(m_particle_type_name,
                                                              new_positions.size());
  new_particles.set<float3>("Position", new_positions);
  new_particles.set<float3>("Velocity", new_velocities);
  new_particles.set<float>("Size", new_sizes);
  new_particles.set<float>("Birth Time", birth_times);
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

  Vector<float3> positions;
  Vector<float3> velocities;
  Vector<float> sizes;
  Vector<float> birth_times;

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

  auto new_particles = interface.particle_allocator().request(m_particle_type_name,
                                                              positions.size());
  new_particles.set<float3>("Position", positions);
  new_particles.set<float3>("Velocity", velocities);
  new_particles.set<float>("Size", sizes);
  new_particles.set<float>("Birth Time", birth_times);

  m_action->execute_from_emitter(new_particles, interface);
}

void CustomFunctionEmitter::emit(EmitterInterface &interface)
{
  TupleCallBody &body = m_function->body<TupleCallBody>();

  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

  body.set_input<float>(fn_in, 0, "Start Time", interface.time_span().start());
  body.set_input<float>(fn_in, 1, "Time Step", interface.time_span().duration());
  body.call__setup_execution_context(fn_in, fn_out);

  auto &float_list_type = FN::Types::GET_TYPE_float_list();
  auto &float3_list_type = FN::Types::GET_TYPE_float3_list();
  auto &int32_list_type = FN::Types::GET_TYPE_int32_list();
  auto &rgba_f_list_type = FN::Types::GET_TYPE_rgba_f_list();
  auto &float_type = FN::Types::GET_TYPE_float();
  auto &float3_type = FN::Types::GET_TYPE_float3();
  auto &int32_type = FN::Types::GET_TYPE_int32();
  auto &rgba_f_type = FN::Types::GET_TYPE_rgba_f();

  uint new_particle_amount = 0;
  for (uint i = 0; i < m_function->output_amount(); i++) {
    auto &type = m_function->output_type(i);
    uint length = 0;
    if (type == float_list_type) {
      length = fn_out.get_ref<SharedFloatList>(i)->size();
    }
    else if (type == float3_list_type) {
      auto &list = fn_out.get_ref<SharedFloat3List>(i);
      length = list->size();
    }
    new_particle_amount = std::max(new_particle_amount, length);
  }

  auto new_particles = interface.particle_allocator().request(m_particle_type_name,
                                                              new_particle_amount);
  new_particles.fill<float>("Birth Time", interface.time_span().end());

  for (uint i = 0; i < m_function->output_amount(); i++) {
    auto &type = m_function->output_type(i);
    StringRef attribute_name = m_function->output_name(i);
    int attribute_index = new_particles.attributes_info().attribute_index_try(attribute_name);

    if (attribute_index == -1) {
      continue;
    }

    if (type == float_list_type) {
      auto list = fn_out.relocate_out<SharedFloatList>(i);
      new_particles.set_repeated<float>(attribute_index, *list.ptr());
    }
    else if (type == float3_list_type) {
      auto list = fn_out.relocate_out<SharedFloat3List>(i);
      new_particles.set_repeated<float3>(attribute_index, *list.ptr());
    }
    else if (type == int32_list_type) {
      auto list = fn_out.relocate_out<SharedInt32List>(i);
      new_particles.set_repeated<int32_t>(attribute_index, *list.ptr());
    }
    else if (type == rgba_f_list_type) {
      auto list = fn_out.relocate_out<SharedFloatRGBAList>(i);
      new_particles.set_repeated<rgba_f>(attribute_index, *list.ptr());
    }
    else if (type == float_type) {
      new_particles.fill<float>(attribute_index, fn_out.get<float>(i));
    }
    else if (type == float3_type) {
      new_particles.fill<float3>(attribute_index, fn_out.get<float3>(i));
    }
    else if (type == int32_type) {
      new_particles.fill<int32_t>(attribute_index, fn_out.get<int32_t>(i));
    }
    else if (type == rgba_f_type) {
      new_particles.fill<rgba_f>(attribute_index, fn_out.get<rgba_f>(i));
    }
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

  auto new_particles = interface.particle_allocator().request(m_particle_type_name,
                                                              new_positions.size());
  new_particles.set<float3>("Position", new_positions);
  new_particles.fill<float>("Birth Time", interface.time_span().start());
  new_particles.fill<float>("Size", m_size);
}

}  // namespace BParticles
