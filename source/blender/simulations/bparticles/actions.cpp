#include "actions.hpp"

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

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());

  for (uint pindex : interface.pindices()) {
    float3 velocity = inputs.get_single<float3>("Velocity", 0, pindex);
    velocities[pindex] = velocity;
  }

  update_position_and_velocity_offsets(interface);
}

void RandomizeVelocityAction::execute(ActionInterface &interface)
{
  auto velocities = interface.attributes().get<float3>("Velocity");

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());

  for (uint pindex : interface.pindices()) {
    float randomness = inputs.get_single<float>("Randomness", 0, pindex);
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

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());
  for (uint pindex : interface.pindices()) {
    rgba_f color = inputs.get_single<rgba_f>("Color", 0, pindex);
    colors[pindex] = color;
  }
}

void ChangeSizeAction::execute(ActionInterface &interface)
{
  auto sizes = interface.attributes().get<float>("Size");

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());
  for (uint pindex : interface.pindices()) {
    float size = inputs.get_single<float>("Size", 0, pindex);
    sizes[pindex] = size;
  }
}

void ChangePositionAction::execute(ActionInterface &interface)
{
  auto positions = interface.attributes().get<float3>("Position");

  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());
  for (uint pindex : interface.pindices()) {
    float3 position = inputs.get_single<float3>("Position", 0, pindex);
    positions[pindex] = position;
  }
}

void KillAction::execute(ActionInterface &interface)
{
  interface.kill(interface.pindices());
}

void ConditionAction::execute(ActionInterface &interface)
{
  auto inputs = ParticleFunctionResult::Compute(
      *m_inputs_fn, interface.pindices(), interface.attributes());

  Vector<uint> true_pindices, false_pindices;
  for (uint pindex : interface.pindices()) {
    if (inputs.get_single<bool>("Condition", 0, pindex)) {
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

  auto inputs = ParticleFunctionResult::Compute(
      m_inputs_fn, interface.pindices(), interface.attributes());

  for (uint pindex : interface.pindices()) {
    const void *value = inputs.get_single("Value", 0, pindex);
    void *dst = attribute[pindex];
    m_attribute_type.copy_to_initialized(value, dst);
  }
}

using FN::MFDataType;
using FN::MFParamType;

void SpawnParticlesAction::execute(ActionInterface &interface)
{
  if (interface.pindices().size() == 0) {
    return;
  }

  uint array_size = interface.pindices().last() + 1;

  auto inputs = ParticleFunctionResult::Compute(
      m_spawn_function, interface.pindices(), interface.attributes());

  LargeScopedArray<int> particle_counts(array_size, -1);

  const MultiFunction &fn = m_spawn_function.fn();
  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    if (param_type.is_vector_output()) {
      FN::GenericVectorArray &vector_array = inputs.computed_vector_array(param_index);
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

  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    StringRef attribute_name = m_attribute_names[param_index];

    switch (data_type.category()) {
      case MFDataType::Single: {
        const FN::CPPType &type = data_type.single__cpp_type();
        void *buffer = MEM_malloc_arrayN(total_spawn_amount, type.size(), __func__);
        GenericMutableArrayRef array(type, buffer, total_spawn_amount);
        GenericArrayRef computed_array = inputs.computed_array(param_index);

        uint current = 0;
        for (uint i : interface.pindices()) {
          uint amount = particle_counts[i];
          array.slice(current, amount).fill__uninitialized(computed_array[i]);
          current += amount;
        }

        attribute_arrays.add(attribute_name, array);
        break;
      }
      case MFDataType::Vector: {
        const FN::CPPType &base_type = data_type.vector__cpp_base_type();
        void *buffer = MEM_malloc_arrayN(total_spawn_amount, base_type.size(), __func__);
        GenericMutableArrayRef array(base_type, buffer, total_spawn_amount);
        FN::GenericVectorArray &computed_vector_array = inputs.computed_vector_array(param_index);

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
