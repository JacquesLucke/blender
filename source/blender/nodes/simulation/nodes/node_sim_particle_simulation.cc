#include "node_simulation_util.h"

static bNodeSocketTemplate sim_node_particle_simulation_in[] = {
    {SOCK_EMITTERS, N_("Emitters")},
    {SOCK_EVENTS, N_("Events")},
    {SOCK_FORCES, N_("Forces")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_particle_simulation_out[] = {
    {-1, ""},
};

void register_node_type_sim_particle_simulation()
{
  static bNodeType ntype;

  sim_node_type_base(
      &ntype, SIM_NODE_PARTICLE_SIMULATION, "Particle Simulation", NODE_CLASS_OUTPUT, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_simulation_in, sim_node_particle_simulation_out);
  nodeRegisterType(&ntype);
}
