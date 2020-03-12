#include "node_sim_util.h"

#include "float.h"

static bNodeSocketTemplate sim_node_particle_mesh_emitter_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {SOCK_FLOAT, N_("Rate"), 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX},
    {-1, ""},
};

static bNodeSocketTemplate sim_node_particle_mesh_emitter_out[] = {
    {SOCK_EMITTERS, N_("Emitter")},
    {-1, ""},
};

void register_node_type_sim_particle_mesh_emitter()
{
  static bNodeType ntype;

  sim_node_type_base(&ntype, SIM_NODE_PARTICLE_MESH_EMITTER, "Mesh Emitter", 0, 0);
  node_type_socket_templates(
      &ntype, sim_node_particle_mesh_emitter_in, sim_node_particle_mesh_emitter_out);
  nodeRegisterType(&ntype);
}
