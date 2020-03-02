#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_set_particle_attribute_in[] = {
    {SOCK_STRING, 1, N_("Name")},
    {SOCK_FLOAT, 1, N_("Value"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, 0, ""},
};

static bNodeSocketTemplate sim_node_set_particle_attribute_out[] = {
    {SOCK_CONTROL_FLOW, 0, N_("Execute")},
    {-1, 0, ""},
};

void register_node_type_set_particle_attribute()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_SET_PARTICLE_ATTRIBUTE, "Set Particle Attribute", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_set_particle_attribute_in, sim_node_set_particle_attribute_out);
  nodeRegisterType(&ntype);
}
