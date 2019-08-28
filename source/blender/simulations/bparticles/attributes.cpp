#include "attributes.hpp"

namespace BParticles {

void AttributesDeclaration::join(AttributesDeclaration &other)
{
  for (uint i = 0; i < other.size(); i++) {
    if (m_names.add(other.m_names[i])) {
      m_types.append(other.m_types[i]);
      m_defaults.append(other.m_defaults[i]);
    }
  }
}

void AttributesDeclaration::join(AttributesInfo &other)
{
  for (uint i = 0; i < other.size(); i++) {
    if (m_names.add(other.m_name_by_index[i])) {
      m_types.append(other.m_type_by_index[i]);
      m_defaults.append(other.m_default_by_index[i]);
    }
  }
}

AttributesInfo::AttributesInfo(AttributesDeclaration &builder)
    : m_name_by_index(builder.m_names),
      m_type_by_index(builder.m_types),
      m_default_by_index(builder.m_defaults)
{
  for (int i = 0; i < m_name_by_index.size(); i++) {
    m_index_by_name.add_new(m_name_by_index[i], i);
  }
}

}  // namespace BParticles
