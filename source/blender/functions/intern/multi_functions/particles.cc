#include "particles.h"

#include "FN_multi_function_common_contexts.h"

namespace FN {

MF_ParticleAttributes::MF_ParticleAttributes(Vector<std::string> attribute_names,
                                             Vector<const CPPType *> attribute_types)
    : m_attribute_names(attribute_names), m_attribute_types(attribute_types)
{
  BLI_assert(m_attribute_names.size() == m_attribute_types.size());

  MFSignatureBuilder signature("Particle Attributes");
  signature.depends_on_per_element_context(true);
  for (uint i = 0; i < m_attribute_names.size(); i++) {
    signature.single_output(m_attribute_names[i], *m_attribute_types[i]);
  }
  this->set_signature(signature);
}

void MF_ParticleAttributes::call(MFMask mask, MFParams params, MFContext context) const
{
  auto context_data = context.try_find_per_element<ParticleAttributesContext>();

  for (uint i = 0; i < m_attribute_names.size(); i++) {
    StringRef attribute_name = m_attribute_names[i];
    const CPPType &attribute_type = *m_attribute_types[i];

    GenericMutableArrayRef r_output = params.uninitialized_single_output(0, attribute_name);

    if (context_data.has_value()) {
      AttributesRef attributes = context_data.value().data->attributes;
      Optional<GenericMutableArrayRef> opt_array = attributes.try_get(attribute_name,
                                                                      attribute_type);
      if (opt_array.has_value()) {
        GenericMutableArrayRef array = opt_array.value();
        for (uint i : mask.indices()) {
          attribute_type.copy_to_uninitialized(array[i], r_output[i]);
        }
        return;
      }
    }

    /* Fallback */
    for (uint i : mask.indices()) {
      attribute_type.construct_default(r_output[i]);
    }
  }
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

}  // namespace FN
