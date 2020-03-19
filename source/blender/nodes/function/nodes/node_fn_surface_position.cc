#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_surface_position_in[] = {
    {SOCK_SURFACE_HOOK, N_("Surface Hook")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_surface_position_out[] = {
    {SOCK_VECTOR, N_("Position")},
    {-1, ""},
};

void register_node_type_fn_surface_position()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SURFACE_POSITION, "Surface Position", 0, 0);
  node_type_socket_templates(&ntype, fn_node_surface_position_in, fn_node_surface_position_out);
  nodeRegisterType(&ntype);
}
