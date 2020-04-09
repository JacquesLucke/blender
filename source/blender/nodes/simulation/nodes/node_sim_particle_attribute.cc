#include "BLI_listbase.h"
#include "node_simulation_util.h"

static bNodeSocketTemplate sim_node_particle_attribute_in[] = {
    {SOCK_STRING, N_("Name")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_particle_attribute_out[] = {
    {SOCK_FLOAT, N_("Float")},
    {SOCK_INT, N_("Int")},
    {SOCK_BOOLEAN, N_("Boolean")},
    {SOCK_VECTOR, N_("Vector")},
    {SOCK_RGBA, N_("Color")},
    {SOCK_OBJECT, N_("Object")},
    {SOCK_IMAGE, N_("Image")},
    {-1, ""},
};

static void sim_node_particle_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    nodeSetSocketAvailability(sock, sock->type == node->custom1);
  }
}

void register_node_type_sim_particle_attribute()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_PARTICLE_ATTRIBUTE, "Particle Attribute", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_attribute_in, sim_node_particle_attribute_out);
  node_type_update(&ntype, sim_node_particle_attribute_update);
  nodeRegisterType(&ntype);
}
