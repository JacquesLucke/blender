#include "type.hpp"

namespace FN {

TypeExtension::~TypeExtension()
{
}

Type::~Type()
{
  for (uint i = 0; i < ARRAY_SIZE(m_extensions); i++) {
    if (m_extensions[i] != nullptr) {
      delete m_extensions[i];
    }
  }
}

}  // namespace FN
