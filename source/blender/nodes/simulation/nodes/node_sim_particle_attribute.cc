#include "node_sim_util.h"

static bNodeSocketTemplate sim_node_particle_attribute_in[] = {
    {SOCK_STRING, N_("Name")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_particle_attribute_out[] = {
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

void register_node_type_sim_particle_attribute()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_PARTICLE_ATTRIBUTE, "Particle Attribute", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_attribute_in, sim_node_particle_attribute_out);
  nodeRegisterType(&ntype);
}
