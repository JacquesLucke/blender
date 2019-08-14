#include "FN_types.hpp"

namespace FN {
namespace Types {

Vector<Type *> types_to_free;

void initialize_types(void)
{
  INIT_bool(types_to_free);
  INIT_external(types_to_free);
  INIT_numeric(types_to_free);
}

void uninitialize_types(void)
{
  for (Type *type : types_to_free) {
    delete type;
  }
  types_to_free.clear_and_make_small();
}

}  // namespace Types
}  // namespace FN
