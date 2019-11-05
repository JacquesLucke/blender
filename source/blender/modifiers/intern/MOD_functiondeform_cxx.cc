#include "DNA_modifier_types.h"

#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_context_ids.h"

#include "BLI_math_cxx.h"

#include "DEG_depsgraph_query.h"

using BKE::VirtualNodeTree;
using BKE::VNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::TemporaryVector;
using BLI::Vector;
using FN::MFContext;
using FN::MFContextBuilder;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MFParamsBuilder;

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts)
{
  if (fdmd->function_tree == nullptr) {
    return;
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fdmd->function_tree);

  BKE::VirtualNodeTreeBuilder vtree_builder;
  vtree_builder.add_all_of_node_tree(btree);
  auto vtree = vtree_builder.build();

  BLI::OwnedResources resources;
  auto function = FN::generate_vtree_multi_function(*vtree, resources);

  MFParamsBuilder params(*function, numVerts);
  params.add_readonly_single_input(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_input(&fdmd->control1);
  params.add_readonly_single_input(&fdmd->control2);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_single_output<float3>(output_vectors);

  ArrayRef<float3> input_vertex_locations = ArrayRef<float3>((float3 *)vertexCos, numVerts);

  MFContextBuilder context_builder;
  context_builder.add(
      FN::ContextIDs::vertex_locations,
      (void *)&input_vertex_locations,
      BLI::VirtualListRef<uint>::FromFullArray(IndexRange(numVerts).as_array_ref()));

  function->call(IndexRange(numVerts).as_array_ref(), params.build(), context_builder.build());

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
