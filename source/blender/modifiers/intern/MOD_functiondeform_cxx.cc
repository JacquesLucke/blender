#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"

#include "BLI_math_cxx.h"

using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::TemporaryVector;
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

  BKE::MultiFunction_AddFloat3s function;
  BKE::MultiFunction::ParamsBuilder params;
  params.start_new(function.signature(), numVerts);
  params.add_readonly_array_ref(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  float3 offset = {fdmd->control1, 2, 0};
  params.add_readonly_single_ref(&offset);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_mutable_array_ref<float3>(output_vectors);

  function.call(IndexRange(numVerts).as_array_ref(), params.build());

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
