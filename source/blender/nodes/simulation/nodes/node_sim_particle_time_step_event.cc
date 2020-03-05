#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_particle_time_step_event_in[] = {
    {SOCK_CONTROL_FLOW, 1, N_("Execute")},
    {-1, 0, ""},
};

static bNodeSocketTemplate sim_node_particle_time_step_event_out[] = {
    {SOCK_EVENTS, 0, N_("Event")},
    {-1, 0, ""},
};

void register_node_type_sim_particle_time_step_event()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_PARTICLE_TIME_STEP_EVENT, "Particle Time Step Event", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_time_step_event_in, sim_node_particle_time_step_event_out);
  nodeRegisterType(&ntype);
}
