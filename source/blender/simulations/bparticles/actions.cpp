#include "actions.hpp"
#include "action_contexts.hpp"

#include "BLI_hash.h"

namespace BParticles {

using BLI::rgba_f;

void NoneAction::execute(ActionInterface &UNUSED(interface))
{
}

void ActionSequence::execute(ActionInterface &interface)
{
  for (auto &action : m_actions) {
    action->execute(interface);
  }
}

static float random_number()
{
  static uint number = 0;
  number++;
  return BLI_hash_int_01(number) * 2.0f - 1.0f;
}

static float3 random_direction()
{
  return float3(random_number(), random_number(), random_number());
}

static void update_position_and_velocity_offsets(ActionInterface &interface)
{
  AttributesRef attributes = interface.attributes();
  AttributesRef attribute_offsets = interface.attribute_offsets();

  auto velocities = attributes.get<float3>("Velocity");
  auto position_offsets = attribute_offsets.try_get<float3>("Position");
  auto velocity_offsets = attribute_offsets.try_get<float3>("Velocity");

  for (uint pindex : interface.pindices()) {
    float3 velocity = velocities[pindex];

    if (position_offsets.has_value()) {
      position_offsets.value()[pindex] = velocity * interface.remaining_time_in_step(pindex);
    }
    if (velocity_offsets.has_value()) {
      velocity_offsets.value()[pindex] = float3(0);
    }
  }
}

void SetVelocityAction::execute(ActionInterface &interface)
{
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float3 velocity = inputs->get<float3>("Velocity", 0, pindex);
    velocities[pindex] = velocity;
  }

  update_position_and_velocity_offsets(interface);
}

void RandomizeVelocityAction::execute(ActionInterface &interface)
{
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    float randomness = inputs->get<float>("Randomness", 0, pindex);
    float3 old_velocity = velocities[pindex];
    float old_speed = old_velocity.length();

    float3 velocity_offset = random_direction().normalized() * old_speed * randomness;
    velocities[pindex] += velocity_offset;
  }

  update_position_and_velocity_offsets(interface);
}

void ChangeColorAction::execute(ActionInterface &interface)
{
  auto colors = interface.attributes().get<rgba_f>("Color");

  auto inputs = m_inputs_fn->compute(interface);
  for (uint pindex : interface.pindices()) {
    rgba_f color = inputs->get<rgba_f>("Color", 0, pindex);
    colors[pindex] = color;
  }
}

void ChangeSizeAction::execute(ActionInterface &interface)
{
  auto sizes = interface.attributes().get<float>("Size");

  auto inputs = m_inputs_fn->compute(interface);
  for (uint pindex : interface.pindices()) {
    float size = inputs->get<float>("Size", 0, pindex);
    sizes[pindex] = size;
  }
}

void ChangePositionAction::execute(ActionInterface &interface)
{
  auto positions = interface.attributes().get<float3>("Position");

  auto inputs = m_inputs_fn->compute(interface);
  for (uint pindex : interface.pindices()) {
    float3 position = inputs->get<float3>("Position", 0, pindex);
    positions[pindex] = position;
  }
}

void KillAction::execute(ActionInterface &interface)
{
  interface.kill(interface.pindices());
}

void ExplodeAction::execute(ActionInterface &interface)
{
  auto positions = interface.attributes().get<float3>("Position");

  Vector<float3> new_positions;
  Vector<float3> new_velocities;
  Vector<float> new_birth_times;

  auto inputs = m_inputs_fn->compute(interface);

  for (uint pindex : interface.pindices()) {
    uint parts_amount = std::max(0, inputs->get<int>("Amount", 0, pindex));
    float speed = inputs->get<float>("Speed", 1, pindex);

    new_positions.append_n_times(positions[pindex], parts_amount);
    new_birth_times.append_n_times(interface.current_times()[pindex], parts_amount);

    for (uint j = 0; j < parts_amount; j++) {
      new_velocities.append(random_direction() * speed);
    }
  }

  for (StringRef system_name : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(system_name,
                                                                new_birth_times.size());
    new_particles.set<float3>("Position", new_positions);
    new_particles.set<float3>("Velocity", new_velocities);
    new_particles.fill<float>("Size", 0.1f);
    new_particles.set<float>("Birth Time", new_birth_times);

    m_on_birth_action.execute_for_new_particles(new_particles, interface);
  }
}

void ConditionAction::execute(ActionInterface &interface)
{
  auto inputs = m_inputs_fn->compute(interface);

  Vector<uint> true_pindices, false_pindices;
  for (uint pindex : interface.pindices()) {
    if (inputs->get<bool>("Condition", 0, pindex)) {
      true_pindices.append(pindex);
    }
    else {
      false_pindices.append(pindex);
    }
  }

  m_true_action.execute_for_subset(true_pindices, interface);
  m_false_action.execute_for_subset(false_pindices, interface);
}

void AddToGroupAction::execute(ActionInterface &interface)
{
  auto is_in_group = interface.attributes().get<bool>(m_group_name);
  for (uint pindex : interface.pindices()) {
    is_in_group[pindex] = true;
  }
}

void RemoveFromGroupAction::execute(ActionInterface &interface)
{
  auto is_in_group_optional = interface.attributes().try_get<bool>(m_group_name);
  if (!is_in_group_optional.has_value()) {
    return;
  }

  MutableArrayRef<bool> is_in_group = *is_in_group_optional;
  for (uint pindex : interface.pindices()) {
    is_in_group[pindex] = false;
  }
}

void SetAttributeAction::execute(ActionInterface &interface)
{
  Optional<GenericMutableArrayRef> attribute_opt = interface.attributes().try_get(
      m_attribute_name, m_attribute_type);

  if (!attribute_opt.has_value()) {
    return;
  }

  GenericMutableArrayRef attribute = *attribute_opt;

  auto inputs = m_inputs_fn.compute(interface);
  for (uint pindex : interface.pindices()) {
    void *value = inputs->get("Value", 0, pindex);
    void *dst = attribute[pindex];
    m_attribute_type.copy_to_initialized(value, dst);
  }
}

using FN::MFDataType;
using FN::MFParamType;

void SpawnParticlesAction::execute(ActionInterface &interface)
{
  FN::MFMask mask(interface.pindices());
  uint min_array_size = mask.min_array_size();
  FN::MFParamsBuilder params_builder{m_spawn_function, min_array_size};

  for (uint param_index : m_spawn_function.param_indices()) {
    MFParamType param_type = m_spawn_function.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    BLI_assert(param_type.is_output());
    switch (param_type.data_type().category()) {
      case MFDataType::Single: {
        const FN::CPPType &type = data_type.single__cpp_type();
        void *buffer = MEM_malloc_arrayN(min_array_size, type.size(), __func__);
        FN::GenericMutableArrayRef array{type, buffer, min_array_size};
        params_builder.add_single_output(array);
        break;
      }
      case MFDataType::Vector: {
        const FN::CPPType &base_type = data_type.vector__cpp_base_type();
        FN::GenericVectorArray *vector_array = new FN::GenericVectorArray(base_type,
                                                                          min_array_size);
        params_builder.add_vector_output(*vector_array);
        break;
      }
    }
  }

  FN::ParticleAttributesContext attributes_context = {interface.attributes()};

  FN::MFContextBuilder context_builder;
  context_builder.add_global_context(m_id_data_cache);
  context_builder.add_global_context(m_id_handle_lookup);
  context_builder.add_element_context(attributes_context, IndexRange(min_array_size));

  m_spawn_function.call(mask, params_builder, context_builder);

  LargeScopedArray<int> particle_counts(min_array_size, -1);

  for (uint param_index : m_spawn_function.param_indices()) {
    MFParamType param_type = m_spawn_function.param_type(param_index);
    if (param_type.is_vector_output()) {
      FN::GenericVectorArray &vector_array = params_builder.computed_vector_array(param_index);
      for (uint i : interface.pindices()) {
        FN::GenericArrayRef array = vector_array[i];
        particle_counts[i] = std::max<int>(particle_counts[i], array.size());
      }
    }
  }

  for (uint i : interface.pindices()) {
    if (particle_counts[i] == -1) {
      particle_counts[i] = 1;
    }
  }

  uint total_spawn_amount = 0;
  for (uint i : interface.pindices()) {
    total_spawn_amount += particle_counts[i];
  }

  StringMap<GenericMutableArrayRef> attribute_arrays;

  Vector<float> new_birth_times;
  for (uint i : interface.pindices()) {
    new_birth_times.append_n_times(interface.current_times()[i], particle_counts[i]);
  }
  attribute_arrays.add_new("Birth Time", new_birth_times.as_mutable_ref());

  for (uint param_index : m_spawn_function.param_indices()) {
    MFParamType param_type = m_spawn_function.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    StringRef attribute_name = m_attribute_names[param_index];

    switch (data_type.category()) {
      case MFDataType::Single: {
        const FN::CPPType &type = data_type.single__cpp_type();
        void *buffer = MEM_malloc_arrayN(total_spawn_amount, type.size(), __func__);
        GenericMutableArrayRef array(type, buffer, total_spawn_amount);
        GenericMutableArrayRef computed_array = params_builder.computed_array(param_index);

        uint current = 0;
        for (uint i : interface.pindices()) {
          uint amount = particle_counts[i];
          array.slice(current, amount).fill__uninitialized(computed_array[i]);
          current += amount;
        }

        attribute_arrays.add(attribute_name, array);

        computed_array.destruct_indices(interface.pindices());
        MEM_freeN(computed_array.buffer());
        break;
      }
      case MFDataType::Vector: {
        const FN::CPPType &base_type = data_type.vector__cpp_base_type();
        void *buffer = MEM_malloc_arrayN(total_spawn_amount, base_type.size(), __func__);
        GenericMutableArrayRef array(base_type, buffer, total_spawn_amount);
        FN::GenericVectorArray &computed_vector_array = params_builder.computed_vector_array(
            param_index);

        uint current = 0;
        for (uint pindex : interface.pindices()) {
          uint amount = particle_counts[pindex];
          GenericMutableArrayRef array_slice = array.slice(current, amount);
          GenericArrayRef computed_array = computed_vector_array[pindex];

          if (computed_array.size() == 0) {
            const void *default_buffer = interface.attributes().info().default_of(attribute_name);
            array_slice.fill__uninitialized(default_buffer);
          }
          else if (computed_array.size() == amount) {
            base_type.copy_to_uninitialized_n(
                computed_array.buffer(), array_slice.buffer(), amount);
          }
          else {
            for (uint i : IndexRange(amount)) {
              base_type.copy_to_uninitialized(computed_array[i % computed_array.size()],
                                              array_slice[i]);
            }
          }

          current += amount;
        }

        attribute_arrays.add(attribute_name, array);

        delete &computed_vector_array;
        break;
      }
    }
  }

  for (StringRef system_name : m_systems_to_emit) {
    auto new_particles = interface.particle_allocator().request(system_name, total_spawn_amount);

    attribute_arrays.foreach_key_value_pair(
        [&](StringRef attribute_name, GenericMutableArrayRef array) {
          if (new_particles.info().has_attribute(attribute_name, array.type())) {
            new_particles.set(attribute_name, array);
          }
        });

    m_action.execute_for_new_particles(new_particles, interface);
  }

  attribute_arrays.foreach_key_value_pair(
      [&](StringRef attribute_name, GenericMutableArrayRef array) {
        if (attribute_name != "Birth Time") {
          array.destruct_indices(interface.pindices());
          MEM_freeN(array.buffer());
        }
      });
}

}  // namespace BParticles
