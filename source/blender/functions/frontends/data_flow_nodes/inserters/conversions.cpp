#include "../registry.hpp"

#include "FN_functions.hpp"

namespace FN {
namespace DataFlowNodes {

static void INSERT_base_to_list(VTreeDataGraphBuilder &builder, DFGB_Socket from, DFGB_Socket to)
{
  SharedType &base_type = from.type();
  auto fn = Functions::GET_FN_list_from_element(base_type);
  DFGB_Node *node = builder.insert_function(fn);
  builder.insert_link(from, node->input(0));
  builder.insert_link(node->output(0), to);
}

void register_conversion_inserters(ConversionInserterRegistry &registry)
{
  registry.function("fn_BooleanSocket", "fn_IntegerSocket", Functions::GET_FN_bool_to_int32);
  registry.function("fn_BooleanSocket", "fn_FloatSocket", Functions::GET_FN_bool_to_float);
  registry.function("fn_IntegerSocket", "fn_BooleanSocket", Functions::GET_FN_int32_to_bool);
  registry.function("fn_IntegerSocket", "fn_FloatSocket", Functions::GET_FN_int32_to_float);
  registry.function("fn_FloatSocket", "fn_BooleanSocket", Functions::GET_FN_float_to_bool);
  registry.function("fn_FloatSocket", "fn_IntegerSocket", Functions::GET_FN_float_to_int32);

  registry.function(
      "fn_BooleanListSocket", "fn_IntegerListSocket", Functions::GET_FN_bool_list_to_int32_list);
  registry.function(
      "fn_BooleanListSocket", "fn_FloatListSocket", Functions::GET_FN_bool_list_to_float_list);
  registry.function(
      "fn_IntegerListSocket", "fn_BooleanListSocket", Functions::GET_FN_int32_list_to_bool_list);
  registry.function(
      "fn_IntegerListSocket", "fn_FloatListSocket", Functions::GET_FN_int32_list_to_float_list);
  registry.function(
      "fn_FloatListSocket", "fn_BooleanListSocket", Functions::GET_FN_float_list_to_bool_list);
  registry.function(
      "fn_FloatListSocket", "fn_IntegerListSocket", Functions::GET_FN_float_list_to_int32_list);

  registry.inserter("fn_FloatSocket", "fn_FloatListSocket", INSERT_base_to_list);
  registry.inserter("fn_VectorSocket", "fn_VectorListSocket", INSERT_base_to_list);
  registry.inserter("fn_IntegerSocket", "fn_IntegerListSocket", INSERT_base_to_list);
  registry.inserter("fn_BooleanSocket", "fn_BooleanListSocket", INSERT_base_to_list);
  registry.inserter("fn_ObjectSocket", "fn_ObjectListSocket", INSERT_base_to_list);
  registry.inserter("fn_ColorSocket", "fn_ColorListSocket", INSERT_base_to_list);
}

}  // namespace DataFlowNodes
}  // namespace FN
