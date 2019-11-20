#pragma once

#include "FN_multi_function.h"

#include "BKE_virtual_node_tree.h"

namespace FN {

using BKE::VInputSocket;
using BKE::VirtualNodeTree;
using BKE::VNode;
using BKE::VOutputSocket;

struct VSocketsForMFParam {
  /* VSockets corresponding to a parameter in a multi-function.
   * If it is an input parameter, only the input_vsocket is set.
   * If it is an output parameter, the output_vsocket might be set (possibly both are nullptr).
   * If it is a mutable parameter, the input_vsocket is required and the output_vsocket might be
   * set.
   * */
  const VInputSocket *input_vsocket;
  const VOutputSocket *output_vsocket;
};

class VNodeMFWrapper {
 public:
  const MultiFunction *function;
  Vector<VSocketsForMFParam> param_vsockets;
};

}  // namespace FN
