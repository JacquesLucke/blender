#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_boolean_math_in[] = {
    {SOCK_BOOLEAN, N_("Boolean")},
    {SOCK_BOOLEAN, N_("Boolean")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_boolean_math_out[] = {
    {SOCK_BOOLEAN, N_("Boolean")},
    {-1, ""},
};

void register_node_type_fn_boolean_math()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_BOOLEAN_MATH, "Boolean Math", 0, 0);
  node_type_socket_templates(&ntype, fn_node_boolean_math_in, fn_node_boolean_math_out);
  node_type_label(&ntype, node_boolean_math_label);
  node_type_update(&ntype, node_boolean_math_update);
  nodeRegisterType(&ntype);
}
