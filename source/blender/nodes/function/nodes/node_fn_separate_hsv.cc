#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_separate_hsv_in[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_separate_hsv_out[] = {
    {SOCK_FLOAT, N_("H")},
    {SOCK_FLOAT, N_("S")},
    {SOCK_FLOAT, N_("V")},
    {-1, ""},
};

void register_node_type_fn_separate_hsv()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SEPARATE_HSV, "Separate HSV", 0, 0);
  node_type_socket_templates(&ntype, fn_node_separate_hsv_in, fn_node_separate_hsv_out);
  nodeRegisterType(&ntype);
}
