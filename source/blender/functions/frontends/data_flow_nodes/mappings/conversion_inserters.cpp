#include "registry.hpp"

#include "FN_functions.hpp"

namespace FN {
namespace DataFlowNodes {

static void INSERT_base_to_list(VTreeDataGraphBuilder &builder,
                                BuilderOutputSocket *from,
                                BuilderInputSocket *to)
{
  Type *base_type = from->type();
  auto fn = Functions::GET_FN_list_from_element(base_type);
  BuilderNode *node = builder.insert_function(fn);
  builder.insert_link(from, node->input(0));
  builder.insert_link(node->output(0), to);
}

void REGISTER_conversion_inserters(std::unique_ptr<LinkInserters> &inserters)
{
#define REGISTER_FN(from_type, to_type, fn) \
  inserters->register_conversion_function(from_type, to_type, Functions::GET_FN_##fn)
#define REGISTER_INSERTER(from_type, to_type, fn) \
  inserters->register_conversion_inserter(from_type, to_type, INSERT_base_to_list)

  REGISTER_FN("Boolean", "Float", bool_to_float);
  REGISTER_FN("Boolean", "Integer", bool_to_int32);
  REGISTER_FN("Float", "Boolean", float_to_bool);
  REGISTER_FN("Float", "Integer", float_to_int32);
  REGISTER_FN("Integer", "Boolean", int32_to_bool);
  REGISTER_FN("Integer", "Float", int32_to_float);

  REGISTER_FN("Boolean List", "Float List", bool_list_to_float_list);
  REGISTER_FN("Boolean List", "Integer List", bool_list_to_int32_list);
  REGISTER_FN("Float List", "Boolean List", float_list_to_bool_list);
  REGISTER_FN("Float List", "Integer List", float_list_to_int32_list);
  REGISTER_FN("Integer List", "Boolean List", int32_list_to_bool_list);
  REGISTER_FN("Integer List", "Float List", int32_list_to_float_list);

  REGISTER_INSERTER("Boolean", "Boolean List", INSERT_base_to_list);
  REGISTER_INSERTER("Color", "Color List", INSERT_base_to_list);
  REGISTER_INSERTER("Float", "Float List", INSERT_base_to_list);
  REGISTER_INSERTER("Integer", "Integer List", INSERT_base_to_list);
  REGISTER_INSERTER("Object", "Object List", INSERT_base_to_list);
  REGISTER_INSERTER("Vector", "Vector List", INSERT_base_to_list);

#undef REGISTER_INSERTER
#undef REGISTER_FN
}

}  // namespace DataFlowNodes
}  // namespace FN
