#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_surface_color_in[] = {
    {SOCK_SURFACE_HOOK, N_("Surface Hook")},
    {SOCK_IMAGE, N_("Image")},
    {SOCK_STRING, N_("UV Map")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_surface_color_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

void register_node_type_fn_surface_color()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SURFACE_COLOR, "Surface Color", 0, 0);
  node_type_socket_templates(&ntype, fn_node_surface_color_in, fn_node_surface_color_out);
  nodeRegisterType(&ntype);
}
