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
    if (m_names.add(other.m_names[i])) {
      m_types.append(other.m_types[i]);
      m_defaults.append(other.m_defaults[i]);
    }
  }
}

AttributesInfo::AttributesInfo(AttributesDeclaration &builder)
    : m_names(builder.m_names), m_types(builder.m_types), m_defaults(builder.m_defaults)
{
}

};  // namespace BParticles
