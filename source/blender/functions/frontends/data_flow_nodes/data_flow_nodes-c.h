#ifndef __FUNCTIONS_DATA_FLOW_NODES_C_WRAPPER_H__
#define __FUNCTIONS_DATA_FLOW_NODES_C_WRAPPER_H__

#include "FN_core-c.h"

struct bNodeTree;

#ifdef __cplusplus
extern "C" {
#endif

FnFunction FN_tree_to_function(struct bNodeTree *bnodetree);

FnFunction FN_function_get_with_signature(struct bNodeTree *btree,
                                          FnType *inputs,
                                          FnType *outputs);

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_DATA_FLOW_NODES_C_WRAPPER_H__ */
