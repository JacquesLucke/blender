#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_simulation_time_input_out[] = {
    {SOCK_FLOAT, N_("Simulation Time")},
    {-1, ""},
};

void register_node_type_sim_simulation_time_input()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_SIMULATION_TIME_INPUT, "Simulation Time Input", 0, 0);
  node_type_socket_templates(&ntype, nullptr, sim_node_simulation_time_input_out);
  nodeRegisterType(&ntype);
}
