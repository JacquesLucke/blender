#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"

using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::ArrayRef;
using BLI::Vector;

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

  BKE::MultiFunction_AddFloats function;

  std::array<float, 4> values_a = {1, 2, 3, 4};
  std::array<float, 4> values_b = {2, 6, 34, 1};
  std::array<float, 4> result;

  Vector<BKE::GenericArrayOrSingleRef> array_or_single_refs;
  array_or_single_refs.append(BKE::GenericArrayOrSingleRef::FromArray(ArrayRef<float>(values_a)));
  array_or_single_refs.append(BKE::GenericArrayOrSingleRef::FromArray(ArrayRef<float>(values_b)));

  Vector<BKE::GenericMutableArrayRef> mutable_array_refs;
  mutable_array_refs.append(BKE::GenericMutableArrayRef(ArrayRef<float>(result)));

  BKE::MultiFunction::Params params{
      array_or_single_refs, mutable_array_refs, {}, {}, function.signature()};

  function.call({0, 1, 2, 3}, params);

  std::cout << result[0] << ", " << result[1] << ", " << result[2] << ", " << result[3] << "\n";
}
