#include "FN_types.hpp"

namespace FN {
namespace Types {

void initialize_types(void)
{
  INIT_bool();
  INIT_external();
  INIT_numeric();
}

void free_types(void)
{
  /* TODO */
}

}  // namespace Types
}  // namespace FN
