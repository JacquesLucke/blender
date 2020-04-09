#include "node_simulation_util.h"

static bNodeSocketTemplate sim_node_emit_particle_in[] = {
    {SOCK_INT, N_("Amount"), 10, 0, 0, 0, 0, 10000000},
    {SOCK_CONTROL_FLOW, N_("Execute")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_emit_particle_out[] = {
    {SOCK_CONTROL_FLOW, N_("Execute")},
    {SOCK_EMITTERS, N_("Emitter")},
    {-1, ""},
};

void register_node_type_sim_emit_particles()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_EMIT_PARTICLES, "Emit Particles", 0, 0);
  node_type_socket_templates(&ntype, sim_node_emit_particle_in, sim_node_emit_particle_out);
  nodeRegisterType(&ntype);
}
