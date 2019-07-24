#include "tuple.hpp"

namespace FN {

TupleMeta::TupleMeta(ArrayRef<SharedType> types) : m_types(types)
{
  m_all_trivially_destructible = true;
  m_size__data = 0;
  for (const SharedType &type : types) {
    CPPTypeInfo &info = type->extension<CPPTypeInfo>();
    m_offsets.append(m_size__data);
    m_type_info.append(&info);
    m_size__data += info.size_of_type();
    if (!info.trivially_destructible()) {
      m_all_trivially_destructible = false;
    }
  }
  m_offsets.append(m_size__data);

  m_size__data_and_init = m_size__data + this->element_amount();
}

void Tuple::print_initialized(std::string name)
{
  std::cout << "Tuple: " << name << std::endl;
  for (uint i = 0; i < m_meta->element_amount(); i++) {
    std::cout << "  Initialized " << i << ": " << m_initialized[i] << std::endl;
  }
}

} /* namespace FN */
