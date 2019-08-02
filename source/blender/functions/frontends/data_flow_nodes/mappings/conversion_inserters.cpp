#include "registry.hpp"

#include "FN_functions.hpp"

namespace FN {
namespace DataFlowNodes {

static void INSERT_base_to_list(VTreeDataGraphBuilder &builder,
                                BuilderOutputSocket *from,
                                BuilderInputSocket *to)
{
  SharedType &base_type = from->type();
  auto fn = Functions::GET_FN_list_from_element(base_type);
  BuilderNode *node = builder.insert_function(fn);
  builder.insert_link(from, node->input(0));
  builder.insert_link(node->output(0), to);
}

void REGISTER_conversion_inserters(ConversionInserterRegistry &registry)
{
  registry.function("Boolean", "Integer", Functions::GET_FN_bool_to_int32);
  registry.function("Boolean", "Float", Functions::GET_FN_bool_to_float);
  registry.function("Integer", "Boolean", Functions::GET_FN_int32_to_bool);
  registry.function("Integer", "Float", Functions::GET_FN_int32_to_float);
  registry.function("Float", "Boolean", Functions::GET_FN_float_to_bool);
  registry.function("Float", "Integer", Functions::GET_FN_float_to_int32);

  registry.function("Boolean List", "Integer List", Functions::GET_FN_bool_list_to_int32_list);
  registry.function("Boolean List", "Float List", Functions::GET_FN_bool_list_to_float_list);
  registry.function("Integer List", "Boolean List", Functions::GET_FN_int32_list_to_bool_list);
  registry.function("Integer List", "Float List", Functions::GET_FN_int32_list_to_float_list);
  registry.function("Float List", "Boolean List", Functions::GET_FN_float_list_to_bool_list);
  registry.function("Float List", "Integer List", Functions::GET_FN_float_list_to_int32_list);

  registry.inserter("Float", "Float List", INSERT_base_to_list);
  registry.inserter("Vector", "Vector List", INSERT_base_to_list);
  registry.inserter("Integer", "Integer List", INSERT_base_to_list);
  registry.inserter("Boolean", "Boolean List", INSERT_base_to_list);
  registry.inserter("Object", "Object List", INSERT_base_to_list);
  registry.inserter("Color", "Color List", INSERT_base_to_list);
}

}  // namespace DataFlowNodes
}  // namespace FN
