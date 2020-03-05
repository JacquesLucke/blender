#include "node_fn_util.h"
#include "node_util.h"

bool fn_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
  /* Function nodes are only supported in simulation node trees so far. */
  return STREQ(ntree->idname, "SimulationNodeTree");
}

void fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = fn_node_poll_default;
}
