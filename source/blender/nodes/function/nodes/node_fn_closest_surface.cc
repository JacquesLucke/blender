#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_closest_surface_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_closest_surface_out[] = {
    {SOCK_SURFACE_HOOK, N_("Closest Hook")},
    {SOCK_VECTOR, N_("Closest Position")},
    {SOCK_VECTOR, N_("Closest Normal")},
    {-1, ""},
};

void register_node_type_fn_closest_surface()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_CLOSEST_SURFACE, "Closest Surface", 0, 0);
  node_type_socket_templates(&ntype, fn_node_closest_surface_in, fn_node_closest_surface_out);
  nodeRegisterType(&ntype);
}
