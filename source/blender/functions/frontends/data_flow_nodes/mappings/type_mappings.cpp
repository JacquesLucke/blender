#include "BLI_lazy_init.hpp"
#include "FN_types.hpp"

#include "registry.hpp"

namespace FN {
namespace DataFlowNodes {

void REGISTER_type_mappings(std::unique_ptr<TypeMappings> &type_mappings)
{
#define ADD_TYPE(idname, name, cpp_name) \
  type_mappings->register_type(idname, name, Types::TYPE_##cpp_name)

  ADD_TYPE("fn_BooleanListSocket", "Boolean List", bool_list);
  ADD_TYPE("fn_BooleanSocket", "Boolean", bool);
  ADD_TYPE("fn_ColorListSocket", "Color List", rgba_f_list);
  ADD_TYPE("fn_ColorSocket", "Color", rgba_f);
  ADD_TYPE("fn_FloatListSocket", "Float List", float_list);
  ADD_TYPE("fn_FloatSocket", "Float", float);
  ADD_TYPE("fn_IntegerListSocket", "Integer List", int32_list);
  ADD_TYPE("fn_IntegerSocket", "Integer", int32);
  ADD_TYPE("fn_ObjectListSocket", "Object List", object_list);
  ADD_TYPE("fn_ObjectSocket", "Object", object);
  ADD_TYPE("fn_VectorListSocket", "Vector List", float3_list);
  ADD_TYPE("fn_VectorSocket", "Vector", float3);

#undef ADD_TYPE
}

}  // namespace DataFlowNodes
}  // namespace FN
