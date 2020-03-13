#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_separate_xyz_in[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_separate_xyz_out[] = {
    {SOCK_FLOAT, N_("X")},
    {SOCK_FLOAT, N_("Y")},
    {SOCK_FLOAT, N_("Z")},
    {-1, ""},
};

void register_node_type_fn_separate_xyz()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SEPARATE_XYZ, "Separate XYZ", 0, 0);
  node_type_socket_templates(&ntype, fn_node_separate_xyz_in, fn_node_separate_xyz_out);
  nodeRegisterType(&ntype);
}
