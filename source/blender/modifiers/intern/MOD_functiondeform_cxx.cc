#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"

using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts)
{
  if (fdmd->function_tree == nullptr) {
    return;
  }

  VirtualNodeTree vtree;
  vtree.add_all_of_tree(fdmd->function_tree);
  vtree.freeze_and_index();

  VirtualNode *output_node = vtree.nodes_with_idname("fn_FunctionOutputNode")[0];

  for (uint i = 0; i < numVerts; i++) {
    vertexCos[i][2] += 3;
  }
}