#include "BKE_node.h"

#include "NOD_simulation.h"

#include "node_sim_util.h"
#include "NOD_common.h"
#include "node_common.h"

void register_node_type_sim_group(void)
{
  static bNodeType ntype;

  node_type_base_custom(
      &ntype, "SimulationNodeGroup", "Group", NODE_CLASS_GROUP, NODE_CONST_OUTPUT);
  ntype.type = NODE_GROUP;
  ntype.poll = sim_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.update_internal_links = node_update_internal_links_default;
  ntype.ext.srna = RNA_struct_find("SimulationNodeGroup");
  BLI_assert(ntype.ext.srna != NULL);
  RNA_struct_blender_type_set(ntype.ext.srna, &ntype);

  node_type_socket_templates(&ntype, NULL, NULL);
  node_type_size(&ntype, 140, 60, 400);
  node_type_label(&ntype, node_group_label);
  node_type_group_update(&ntype, node_group_update);

  nodeRegisterType(&ntype);
}