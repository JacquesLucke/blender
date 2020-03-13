#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_combine_hsv_in[] = {
    {SOCK_FLOAT, N_("H"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("S"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("V"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_hsv_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

void register_node_type_fn_combine_hsv()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_HSV, "Combine HSV", 0, 0);
  node_type_socket_templates(&ntype, fn_node_combine_hsv_in, fn_node_combine_hsv_out);
  nodeRegisterType(&ntype);
}
