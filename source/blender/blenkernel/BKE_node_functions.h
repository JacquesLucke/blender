#ifndef __BKE_NODE_FUNCTIONS_H__
#define __BKE_NODE_FUNCTIONS_H__

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_cpp_function.h"

#include "BLI_optional.h"

namespace BKE {

using BLI::Optional;

struct FunctionForNode {
  CPPFunction *function;
  bool is_newly_allocated;
};

Optional<FunctionForNode> get_vnode_array_function(VirtualNode *vnode);

void init_vnode_array_functions();

};  // namespace BKE

#endif /* __BKE_NODE_FUNCTIONS_H__ */
