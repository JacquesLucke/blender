#include "DNA_modifier_types.h"

#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"

#include "BLI_math_cxx.h"

#include "BKE_modifier.h"
#include "BKE_id_data_cache.h"

#include "DEG_depsgraph_query.h"

using BKE::VNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::LargeScopedVector;
using BLI::Vector;
using FN::FunctionTree;
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

  FN::BTreeVTreeMap vtrees;
  FunctionTree function_tree(btree, vtrees);

  BLI::ResourceCollector resources;
  auto function = FN::MFGeneration::generate_node_tree_multi_function(function_tree, resources);

  MFParamsBuilder params_builder(*function, numVerts);
  params_builder.add_readonly_single_input(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params_builder.add_readonly_single_input(&fdmd->control1);
  params_builder.add_readonly_single_input(&fdmd->control2);

  LargeScopedVector<float3> output_vectors(numVerts);
  params_builder.add_single_output<float3>(output_vectors);

  float current_time = DEG_get_ctime(ctx->depsgraph);

  FN::SceneTimeContext time_context;
  time_context.time = current_time;

  FN::VertexPositionArray vertex_positions_context;
  vertex_positions_context.positions = ArrayRef<float3>((float3 *)vertexCos, numVerts);

  BKE::IDHandleLookup id_handle_lookup;
  FN::add_ids_used_by_nodes(id_handle_lookup, function_tree);

  BKE::IDDataCache id_data_cache;

  MFContextBuilder context_builder;
  context_builder.add_global_context(id_handle_lookup);
  context_builder.add_global_context(time_context);
  context_builder.add_global_context(id_data_cache);
  context_builder.add_element_context(vertex_positions_context,
                                      FN::MFElementContextIndices::FromDirectMapping());

  function->call(IndexRange(numVerts), params_builder, context_builder);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
