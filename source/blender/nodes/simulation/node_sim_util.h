#ifndef __NODE_SHADER_UTIL_H__
#define __NODE_SHADER_UTIL_H__

#include <string.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "NOD_simulation.h"

#include "node_util.h"

void sim_node_type_base(
    struct bNodeType *ntype, int type, const char *name, short nclass, short flag);
bool sim_node_poll_default(struct bNodeType *ntype, struct bNodeTree *ntree);

#endif /* __NODE_SHADER_UTIL_H__ */
