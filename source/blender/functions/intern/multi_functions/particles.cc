#include "particles.h"
#include "util.h"

#include "FN_multi_function_common_contexts.h"

namespace FN {

MF_ParticleAttribute::MF_ParticleAttribute(const CPPType &type) : m_type(type)
{
  MFSignatureBuilder signature = this->get_builder("Particle Attribute");
  signature.use_element_context<ParticleAttributesContext>();
  signature.single_input<std::string>("Attribute Name");
  signature.single_output("Value", type);
}

void MF_ParticleAttribute::call(IndexMask mask, MFParams params, MFContext context) const
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

MF_EmitterTimeInfo::MF_EmitterTimeInfo()
{
  MFSignatureBuilder signature = this->get_builder("Emitter Time Info");
  signature.use_global_context<EmitterTimeInfoContext>();
  signature.single_output<float>("Duration");
  signature.single_output<float>("Begin");
  signature.single_output<float>("End");
  signature.single_output<int>("Step");
}

void MF_EmitterTimeInfo::call(IndexMask mask, MFParams params, MFContext context) const
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

MF_EventFilterEndTime::MF_EventFilterEndTime()
{
  MFSignatureBuilder signature = this->get_builder("Event Filter End Time");
  signature.use_global_context<EventFilterEndTimeContext>();
  signature.single_output<float>("End Time");
}

void MF_EventFilterEndTime::call(IndexMask mask, MFParams params, MFContext context) const
{
  MutableArrayRef<float> end_times = params.uninitialized_single_output<float>(0, "End Time");

  auto *time_context = context.try_find_global<EventFilterEndTimeContext>();
  if (time_context == nullptr) {
    end_times.fill_indices(mask.indices(), 0.0f);
  }
  else {
    end_times.fill_indices(mask.indices(), time_context->end_time);
  }
}

}  // namespace FN
