#include "DNA_modifier_types.h"

#include "FN_inlined_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"

#include "BLI_math_cxx.h"

#include "BKE_modifier.h"

#include "DEG_depsgraph_query.h"

using BKE::InlinedNodeTree;
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
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd,
                           float (*vertexCos)[3],
                           int numVerts,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh);
}

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd,
                           float (*vertexCos)[3],
                           int numVerts,
                           const ModifierEvalContext *ctx,
                           Mesh *UNUSED(mesh))
{
  if (fdmd->function_tree == nullptr) {
    return;
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fdmd->function_tree);

  BKE::BTreeVTreeMap vtrees;
  InlinedNodeTree inlined_tree(btree, vtrees);

  BLI::ResourceCollector resources;
  auto function = FN::generate_inlined_tree_multi_function(inlined_tree, resources);

  MFParamsBuilder params_builder(*function, numVerts);
  params_builder.add_readonly_single_input(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params_builder.add_readonly_single_input(&fdmd->control1);
  params_builder.add_readonly_single_input(&fdmd->control2);

  TemporaryVector<float3> output_vectors(numVerts);
  params_builder.add_single_output<float3>(output_vectors);

  float current_time = DEG_get_ctime(ctx->depsgraph);

  FN::SceneTimeContext time_context;
  time_context.time = current_time;

  FN::VertexPositionArray vertex_positions_context;
  vertex_positions_context.positions = ArrayRef<float3>((float3 *)vertexCos, numVerts);

  BKE::IDHandleLookup id_handle_lookup;
  FN::add_objects_used_by_inputs(id_handle_lookup, inlined_tree);

  MFContextBuilder context_builder(&id_handle_lookup);
  context_builder.add_element_context(time_context);
  context_builder.add_element_context(
      vertex_positions_context,
      BLI::VirtualListRef<uint>::FromFullArray(IndexRange(numVerts).as_array_ref()));

  function->call(IndexRange(numVerts), params_builder, context_builder);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
