#ifndef __NODE_SIMULATION_UTIL_H__
#define __NODE_SIMULATION_UTIL_H__

#include "node_sim_util.h"
#include "node_util.h"

void sim_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = [](bNodeType *UNUSED(ntype), bNodeTree *ntree) {
    return STREQ(ntree->idname, "SimulationNodeTree");
  };
}

bool sim_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
  return STREQ(ntree->idname, "SimulationNodeTree");
}

#endif /* __NODE_SIMULATION_UTIL_H__ */