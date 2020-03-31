#include "node_simulation_util.h"
#include "node_util.h"

bool sim_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
  return STREQ(ntree->idname, "SimulationNodeTree");
}

void sim_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = sim_node_poll_default;
}
