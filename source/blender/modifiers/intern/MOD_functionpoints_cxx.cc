#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "BLI_math.h"

#include "FN_inlined_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

using BKE::InlinedNodeTree;
using BKE::VNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::Vector;
using FN::MFContext;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MFParamsBuilder;

extern "C" {
Mesh *MOD_functionpoints_do(FunctionPointsModifierData *fpmd,
                            const struct ModifierEvalContext *ctx);
}

Mesh *MOD_functionpoints_do(FunctionPointsModifierData *fpmd,
                            const struct ModifierEvalContext *ctx)
{
  if (fpmd->function_tree == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  BKE::BTreeVTreeMap vtrees;
  InlinedNodeTree inlined_tree(btree, vtrees);

  BLI::ResourceCollector resources;
  auto function = FN::generate_inlined_tree_multi_function(inlined_tree, resources);

  MFParamsBuilder params_builder(*function, 1);
  params_builder.add_readonly_single_input(&fpmd->control1);
  params_builder.add_readonly_single_input(&fpmd->control2);

  FN::GenericVectorArray vector_array{FN::CPP_TYPE<float3>(), 1};
  params_builder.add_vector_output(vector_array);

  FN::SceneTimeContext time_context;
  time_context.time = DEG_get_ctime(ctx->depsgraph);

  BKE::IDHandleLookup id_handle_lookup;
  FN::add_ids_used_by_nodes(id_handle_lookup, inlined_tree);

  FN::MFContextBuilder context_builder;
  context_builder.add_global_context(id_handle_lookup);
  context_builder.add_global_context(time_context);

  function->call({0}, params_builder, context_builder);

  ArrayRef<float3> output_points = vector_array[0].as_typed_ref<float3>();

  Mesh *mesh = BKE_mesh_new_nomain(output_points.size(), 0, 0, 0, 0);
  for (uint i = 0; i < output_points.size(); i++) {
    copy_v3_v3(mesh->mvert[i].co, output_points[i]);
  }

  return mesh;
}
