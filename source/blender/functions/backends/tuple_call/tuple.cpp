#include "tuple.hpp"

namespace FN {

void Tuple::print_initialized(std::string name)
{
  std::cout << "Tuple: " << name << std::endl;
  for (uint i = 0; i < m_meta->element_amount(); i++) {
    std::cout << "  Initialized " << i << ": " << m_initialized[i] << std::endl;
  }
}

} /* namespace FN */
