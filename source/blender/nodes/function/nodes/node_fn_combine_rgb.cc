#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_combine_rgb_in[] = {
    {SOCK_FLOAT, N_("R"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("G"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("B"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_rgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

void register_node_type_fn_combine_rgb()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_RGB, "Combine RGB", 0, 0);
  node_type_socket_templates(&ntype, fn_node_combine_rgb_in, fn_node_combine_rgb_out);
  nodeRegisterType(&ntype);
}
