#include "parameter.hpp"

namespace FN {

void Parameter::print() const
{
  std::cout << this->type()->name() << " - " << this->name();
}

} /* namespace FN */
