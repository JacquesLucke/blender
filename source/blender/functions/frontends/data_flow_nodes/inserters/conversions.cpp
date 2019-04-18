#include "../registry.hpp"

#include "FN_functions.hpp"

namespace FN {
namespace DataFlowNodes {

static void INSERT_base_to_list(GraphBuilder &builder,
                                Socket from,
                                Socket to,
                                struct bNodeLink *UNUSED(source_link))
{
  SharedType &base_type = from.type();
  auto fn = Functions::GET_FN_list_from_element(base_type);
  Node *node = builder.insert_function(fn);
  builder.insert_link(from, node->input(0));
  builder.insert_link(node->output(0), to);
}

void register_conversion_inserters(GraphInserters &inserters)
{
  inserters.reg_conversion_function("Integer", "Float", Functions::GET_FN_int32_to_float);
  inserters.reg_conversion_function("Float", "Integer", Functions::GET_FN_float_to_int32);

  inserters.reg_conversion_inserter("Float", "Float List", INSERT_base_to_list);
  inserters.reg_conversion_inserter("Vector", "Vector List", INSERT_base_to_list);
  inserters.reg_conversion_inserter("Integer", "Integer List", INSERT_base_to_list);

  inserters.reg_conversion_function(
      "Float List", "Integer List", Functions::GET_FN_float_list_to_int32_list);
  inserters.reg_conversion_function(
      "Integer List", "Float List", Functions::GET_FN_int32_list_to_float_list);
}

}  // namespace DataFlowNodes
}  // namespace FN
