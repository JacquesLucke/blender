#include "NOD_simulation.h"

#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_particle_simulation_in[] = {
    {SOCK_EMITTERS, 1, N_("Emitters"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {SOCK_EVENTS, 1, N_("Events"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {SOCK_FORCES, 1, N_("Forces"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {SOCK_CONTROL_FLOW, 1, N_("Control Flow"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {-1, 0, ""},
};

static bNodeSocketTemplate sim_node_particle_simulation_out[] = {
    {-1, 0, ""},
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