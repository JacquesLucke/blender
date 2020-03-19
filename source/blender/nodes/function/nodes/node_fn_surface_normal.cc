#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_surface_normal_in[] = {
    {SOCK_SURFACE_HOOK, N_("Surface Hook")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_surface_normal_out[] = {
    {SOCK_VECTOR, N_("Normal")},
    {-1, ""},
};

void register_node_type_fn_surface_normal()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SURFACE_NORMAL, "Surface Normal", 0, 0);
  node_type_socket_templates(&ntype, fn_node_surface_normal_in, fn_node_surface_normal_out);
  nodeRegisterType(&ntype);
}
