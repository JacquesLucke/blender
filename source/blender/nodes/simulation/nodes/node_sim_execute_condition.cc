#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_execute_condition_in[] = {
    {SOCK_BOOLEAN, 1, N_("Condition")},
    {SOCK_CONTROL_FLOW, 1, N_("If True")},
    {SOCK_CONTROL_FLOW, 1, N_("If False")},
    {-1, 0, ""},
};

static bNodeSocketTemplate sim_node_execute_condition_out[] = {
    {SOCK_CONTROL_FLOW, 0, N_("Execute")},
    {-1, 0, ""},
};

void register_node_type_sim_execute_condition()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_EXECUTE_CONDITION, "Execute Condition", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_execute_condition_in, sim_node_execute_condition_out);
  nodeRegisterType(&ntype);
}
