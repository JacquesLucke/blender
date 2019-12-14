#include "particles.h"
#include "util.h"

#include "FN_multi_function_common_contexts.h"

namespace FN {

MF_ParticleAttribute::MF_ParticleAttribute(const CPPType &type) : m_type(type)
{
  MFSignatureBuilder signature("Particle Attribute");
  signature.depends_on_per_element_context(true);
  signature.single_input<std::string>("Attribute Name");
  signature.single_output("Value", type);
  this->set_signature(signature);
}

void MF_ParticleAttribute::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<std::string> attribute_names = params.readonly_single_input<std::string>(
      0, "Attribute Name");
  GenericMutableArrayRef r_values = params.uninitialized_single_output(1, "Value");

  auto context_data = context.try_find_per_element<ParticleAttributesContext>();
  if (!context_data.has_value()) {
    r_values.default_initialize(mask.indices());
    return;
  }

  AttributesRef attributes = context_data->data->attributes;
  VirtualListRef<uint> element_indices = context_data->indices;

  group_indices_by_same_value(
      mask.indices(),
      attribute_names,
      [&](StringRef attribute_name, ArrayRef<uint> indices_with_same_name) {
        Optional<GenericMutableArrayRef> opt_array = attributes.try_get(attribute_name, m_type);
        if (!opt_array.has_value()) {
          r_values.default_initialize(indices_with_same_name);
          return;
        }
        GenericMutableArrayRef array = opt_array.value();
        for (uint i : indices_with_same_name) {
          uint index = element_indices[i];
          r_values.copy_in__initialized(i, array[index]);
        }
      });
}

MF_ParticleIsInGroup::MF_ParticleIsInGroup()
{
  MFSignatureBuilder signature("Particle is in Group");
  signature.depends_on_per_element_context(true);
  signature.single_input<std::string>("Group Name");
  signature.single_output<bool>("Is in Group");
  this->set_signature(signature);
}

void MF_ParticleIsInGroup::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<std::string> group_names = params.readonly_single_input<std::string>(
      0, "Group Name");
  MutableArrayRef<bool> r_is_in_group = params.uninitialized_single_output<bool>(1, "Is in Group");

  auto context_data = context.try_find_per_element<ParticleAttributesContext>();
  if (!context_data.has_value()) {
    r_is_in_group.fill_indices(mask.indices(), false);
    return;
  }

  AttributesRef attributes = context_data->data->attributes;

  for (uint i : mask.indices()) {
    const std::string group_name = group_names[i];
    Optional<MutableArrayRef<bool>> is_in_group_attr = attributes.try_get<bool>(group_name);
    if (!is_in_group_attr.has_value()) {
      r_is_in_group[i] = false;
      continue;
    }

    uint index = context_data->indices[i];
    bool is_in_group = is_in_group_attr.value()[index];
    r_is_in_group[i] = is_in_group;
  }
}

MF_EmitterTimeInfo::MF_EmitterTimeInfo()
{
  MFSignatureBuilder signature("Emitter Time Info");
  signature.single_output<float>("Duration");
  signature.single_output<float>("Begin");
  signature.single_output<float>("End");
  signature.single_output<int>("Step");
  this->set_signature(signature);
}

void MF_EmitterTimeInfo::call(MFMask mask, MFParams params, MFContext context) const
{
  MutableArrayRef<float> r_durations = params.uninitialized_single_output<float>(0, "Duration");
  MutableArrayRef<float> r_begins = params.uninitialized_single_output<float>(1, "Begin");
  MutableArrayRef<float> r_ends = params.uninitialized_single_output<float>(2, "End");
  MutableArrayRef<int> r_steps = params.uninitialized_single_output<int>(3, "Step");

  auto *time_context = context.try_find_global<EmitterTimeInfoContext>();

  ArrayRef<uint> indices = mask.indices();
  if (time_context == nullptr) {
    r_durations.fill_indices(indices, 0.0f);
    r_begins.fill_indices(indices, 0.0f);
    r_ends.fill_indices(indices, 0.0f);
    r_steps.fill_indices(indices, 0);
  }
  else {
    r_durations.fill_indices(indices, time_context->duration);
    r_begins.fill_indices(indices, time_context->begin);
    r_ends.fill_indices(indices, time_context->end);
    r_steps.fill_indices(indices, time_context->step);
  }
}

}  // namespace FN
