#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_surface_weight_in[] = {
    {SOCK_SURFACE_HOOK, N_("Surface Hook")},
    {SOCK_STRING, N_("Group")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_surface_weight_out[] = {
    {SOCK_FLOAT, N_("Weight")},
    {-1, ""},
};

void register_node_type_fn_surface_weight()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SURFACE_WEIGHT, "Surface Position", 0, 0);
  node_type_socket_templates(&ntype, fn_node_surface_weight_in, fn_node_surface_weight_out);
  nodeRegisterType(&ntype);
}
