#include "node_simulation_util.h"

static bNodeSocketTemplate sim_node_force_in[] = {
    {SOCK_VECTOR, N_("Force"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_force_out[] = {
    {SOCK_FORCES, N_("Force")},
    {-1, ""},
};

void register_node_type_sim_force()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_FORCE, "Force", 0, 0);
  node_type_socket_templates(&ntype, sim_node_force_in, sim_node_force_out);
  nodeRegisterType(&ntype);
}
