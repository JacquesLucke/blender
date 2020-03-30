#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_string_concatenation_in[] = {
    {SOCK_STRING, N_("A")},
    {SOCK_STRING, N_("B")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_string_concatenation_out[] = {
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

void register_node_type_fn_string_concatenation()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_STRING_CONCATENATION, "String Concatenation", 0, 0);
  node_type_socket_templates(
      &ntype, fn_node_string_concatenation_in, fn_node_string_concatenation_out);
  nodeRegisterType(&ntype);
}
