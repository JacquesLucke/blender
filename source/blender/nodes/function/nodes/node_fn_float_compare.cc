#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_float_compare_in[] = {
    {SOCK_FLOAT, N_("A"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("B"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("Epsilon"), 0.001f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_float_compare_out[] = {
    {SOCK_BOOLEAN, N_("Result")},
    {-1, ""},
};

void register_node_type_fn_float_compare()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_FLOAT_COMPARE, "Boolean Math", 0, 0);
  node_type_socket_templates(&ntype, fn_node_float_compare_in, fn_node_float_compare_out);
  node_type_label(&ntype, node_float_compare_label);
  node_type_update(&ntype, node_float_compare_update);
  nodeRegisterType(&ntype);
}
