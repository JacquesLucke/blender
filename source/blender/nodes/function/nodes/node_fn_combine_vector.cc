#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_combine_vector_in[] = {
    {SOCK_FLOAT, N_("X"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("Y"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("Z"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_vector_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

void register_node_type_fn_combine_vector()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_VECTOR, "Combine Vector", 0, 0);
  node_type_socket_templates(&ntype, fn_node_combine_vector_in, fn_node_combine_vector_out);
  nodeRegisterType(&ntype);
}
