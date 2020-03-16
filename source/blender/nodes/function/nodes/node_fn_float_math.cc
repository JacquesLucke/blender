#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_float_math_in[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_float_math_out[] = {
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

void register_node_type_fn_float_math()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_FLOAT_MATH, "Float Math", 0, 0);
  node_type_socket_templates(&ntype, fn_node_float_math_in, fn_node_float_math_out);
  node_type_label(&ntype, node_math_label);
  node_type_update(&ntype, node_math_update);
  nodeRegisterType(&ntype);
}
