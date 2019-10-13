#include "BLI_lazy_init_cxx.h"
#include "BLI_math_cxx.h"

#include "BKE_function_nodes_data_types.h"

#include "DNA_object_types.h"

namespace BKE {

void insert_single_and_list_type(StringMap<SocketDataType> &types, StringRef name, CPPType &type)
{
  std::string base_idname = "fn_" + name + "Socket";
  std::string list_idname = "fn_" + name + "ListSocket";

  types.add_new(base_idname, {&type, DataTypeCategory::Single});
  types.add_new(list_idname, {&type, DataTypeCategory::List});
}

BLI_LAZY_INIT(StringMap<SocketDataType>, get_function_nodes_data_types)
{
  StringMap<SocketDataType> types;

  insert_single_and_list_type(types, "Boolean", GET_TYPE<bool>());
  insert_single_and_list_type(types, "Color", GET_TYPE<BLI::rgba_f>());
  insert_single_and_list_type(types, "Float", GET_TYPE<float>());
  insert_single_and_list_type(types, "Integer", GET_TYPE<int32_t>());
  insert_single_and_list_type(types, "Object", GET_TYPE<Object *>());
  insert_single_and_list_type(types, "Text", GET_TYPE<std::string>());
  insert_single_and_list_type(types, "Vector", GET_TYPE<BLI::float3>());

  return types;
}

}  // namespace BKE