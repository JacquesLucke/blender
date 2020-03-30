#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_instance_identifier_out[] = {
    {SOCK_STRING, N_("Identifier")},
    {-1, ""},
};

void register_node_type_fn_instance_identifier()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INSTANCE_IDENTIFIER, "Instance Identifier", 0, 0);
  node_type_socket_templates(&ntype, nullptr, fn_node_instance_identifier_out);
  nodeRegisterType(&ntype);
}
