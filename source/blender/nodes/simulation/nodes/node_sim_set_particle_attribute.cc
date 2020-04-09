#include "BLI_listbase.h"
#include "node_simulation_util.h"

static bNodeSocketTemplate sim_node_set_particle_attribute_in[] = {
    {SOCK_STRING, N_("Name")},
    {SOCK_FLOAT, N_("Float"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_INT, N_("Int"), 0, 0, 0, 0, -10000, 10000},
    {SOCK_BOOLEAN, N_("Boolean")},
    {SOCK_VECTOR, N_("Vector")},
    {SOCK_RGBA, N_("Color")},
    {SOCK_OBJECT, N_("Object")},
    {SOCK_IMAGE, N_("Image")},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_set_particle_attribute_out[] = {
    {SOCK_CONTROL_FLOW, N_("Execute")},
    {-1, ""},
};

static void sim_node_set_particle_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  int index = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (index >= 1) {
      nodeSetSocketAvailability(sock, sock->type == node->custom1);
    }
    index++;
  }
}

void register_node_type_sim_set_particle_attribute()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_SET_PARTICLE_ATTRIBUTE, "Set Particle Attribute", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_set_particle_attribute_in, sim_node_set_particle_attribute_out);
  node_type_update(&ntype, sim_node_set_particle_attribute_update);
  nodeRegisterType(&ntype);
}
