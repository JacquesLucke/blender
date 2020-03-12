#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_particle_birth_event_in[] = {
    {SOCK_CONTROL_FLOW, N_("Execute")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_particle_birth_event_out[] = {
    {SOCK_EVENTS, N_("Event")},
    {-1, ""},
};

void register_node_type_sim_particle_birth_event()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_PARTICLE_BIRTH_EVENT, "Particle Birth Event", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_birth_event_in, sim_node_particle_birth_event_out);
  nodeRegisterType(&ntype);
}
