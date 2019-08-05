#include "numeric_lists.hpp"
#include "BLI_lazy_init.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Types {

BLI_LAZY_INIT(SharedType, GET_TYPE_float_list)
{
  return new_list_type(GET_TYPE_float());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_float3_list)
{
  return new_list_type(GET_TYPE_float3());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_int32_list)
{
  return new_list_type(GET_TYPE_int32());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_bool_list)
{
  return new_list_type(GET_TYPE_bool());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_object_list)
{
  return new_list_type(GET_TYPE_object());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_rgba_f_list)
{
  return new_list_type(GET_TYPE_rgba_f());
}

}  // namespace Types
}  // namespace FN
