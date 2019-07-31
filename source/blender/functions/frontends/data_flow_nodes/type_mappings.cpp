#include "BLI_lazy_init.hpp"

#include "FN_types.hpp"

#include "type_mappings.hpp"

namespace FN {
namespace DataFlowNodes {

struct StringTypeMappings {
  StringMap<SharedType> by_idname;
  StringMap<SharedType> by_type_name;
  StringMap<std::string> data_type_by_idname;
};

BLI_LAZY_INIT_STATIC(StringTypeMappings, get_type_by_name_mappings)
{
#define ADD_TYPE(idname, data_type, func_suffix) \
  maps.by_idname.add_new(idname, Types::GET_TYPE_##func_suffix()); \
  maps.by_type_name.add_new(data_type, Types::GET_TYPE_##func_suffix()); \
  maps.data_type_by_idname.add_new(idname, data_type)

  StringTypeMappings maps;
  ADD_TYPE("fn_FloatSocket", "Float", float);
  ADD_TYPE("fn_FloatListSocket", "Float List", float_list);
  ADD_TYPE("fn_VectorSocket", "Vector", float3);
  ADD_TYPE("fn_VectorListSocket", "Vector List", float3_list);
  ADD_TYPE("fn_IntegerSocket", "Integer", int32);
  ADD_TYPE("fn_IntegerListSocket", "Integer List", int32_list);
  ADD_TYPE("fn_BooleanSocket", "Boolean", bool);
  ADD_TYPE("fn_BooleanListSocket", "Boolean List", bool_list);
  ADD_TYPE("fn_ObjectSocket", "Object", object);
  ADD_TYPE("fn_ObjectListSocket", "Object List", object_list);
  ADD_TYPE("fn_ColorSocket", "Color", rgba_f);
  ADD_TYPE("fn_ColorListSocket", "Color List", rgba_f_list);
  return maps;

#undef ADD_TYPE
}

StringMap<SharedType> &get_type_by_idname_map()
{
  return get_type_by_name_mappings().by_idname;
}

StringMap<SharedType> &get_type_by_data_type_map()
{
  return get_type_by_name_mappings().by_type_name;
}

StringMap<std::string> &get_data_type_by_idname_map()
{
  return get_type_by_name_mappings().data_type_by_idname;
}

}  // namespace DataFlowNodes
}  // namespace FN
