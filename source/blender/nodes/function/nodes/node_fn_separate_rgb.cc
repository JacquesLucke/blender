#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_separate_rgb_in[] = {
    {SOCK_RGBA, N_("Color"), 0.8f, 0.8f, 0.8f, 1.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_separate_rgb_out[] = {
    {SOCK_FLOAT, N_("R")},
    {SOCK_FLOAT, N_("G")},
    {SOCK_FLOAT, N_("B")},
    {-1, ""},
};

void register_node_type_fn_separate_rgb()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SEPARATE_RGB, "Separate RGB", 0, 0);
  node_type_socket_templates(&ntype, fn_node_separate_rgb_in, fn_node_separate_rgb_out);
  nodeRegisterType(&ntype);
}
