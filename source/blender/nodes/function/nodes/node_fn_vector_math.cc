#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_vector_math_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_vector_math_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

void register_node_type_fn_vector_math()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_VECTOR_MATH, "Vector Math", 0, 0);
  node_type_socket_templates(&ntype, fn_node_vector_math_in, fn_node_vector_math_out);
  node_type_label(&ntype, node_vector_math_label);
  node_type_update(&ntype, node_vector_math_update);
  nodeRegisterType(&ntype);
}
