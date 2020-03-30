#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_object_transforms_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_object_transforms_out[] = {
    {SOCK_VECTOR, N_("Location")},
    {-1, ""},
};

void register_node_type_fn_object_transforms()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_OBJECT_TRANSFORMS, "Object Transforms", 0, 0);
  node_type_socket_templates(&ntype, fn_node_object_transforms_in, fn_node_object_transforms_out);
  nodeRegisterType(&ntype);
}
