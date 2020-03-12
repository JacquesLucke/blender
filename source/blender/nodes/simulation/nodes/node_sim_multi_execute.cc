#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_multi_execute_in[] = {
    {SOCK_CONTROL_FLOW, "1"},
    {SOCK_CONTROL_FLOW, "2"},
    {SOCK_CONTROL_FLOW, "3"},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_multi_execute_out[] = {
    {SOCK_CONTROL_FLOW, N_("Execute")},
    {-1, ""},
};

void register_node_type_sim_multi_execute()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_MULTI_EXECUTE, "Multi Execute", 0, 0);
  node_type_socket_templates(&ntype, sim_node_multi_execute_in, sim_node_multi_execute_out);
  nodeRegisterType(&ntype);
}
