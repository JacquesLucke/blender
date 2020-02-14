#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_id_data_cache.h"

#include "BLI_math.h"

#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"
#include "FN_expression_lexer.h"
#include "FN_expression_parser.h"
#include "FN_expression_multi_function.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

using BKE::VNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::Vector;
using FN::FunctionTree;
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
  {
    std::string str = "x*var+5.0";

    FN::Expr::ConstantsTable constants_table;
    constants_table.add_single("var", 100.0f);

    BLI::ResourceCollector resources;
    const FN::MultiFunction &fn = FN::Expr::expression_to_multi_function(
        str, resources, {"x"}, {FN::MFDataType::ForSingle<float>()}, constants_table);

    FN::MFParamsBuilder params_builder(fn, 1);
    FN::MFContextBuilder context_builder;

    float input_x = 2.25f;
    float result;
    params_builder.add_readonly_single_input(&input_x);
    params_builder.add_single_output(&result);
    fn.call(IndexRange(1), params_builder, context_builder);
    std::cout << "Result: " << result << '\n';
  }
  if (fpmd->function_tree == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  FN::BTreeVTreeMap vtrees;
  FunctionTree function_tree(btree, vtrees);

  BLI::ResourceCollector resources;
  auto function = FN::MFGeneration::generate_node_tree_multi_function(function_tree, resources);

  MFParamsBuilder params_builder(*function, 1);
  params_builder.add_readonly_single_input(&fpmd->control1);
  params_builder.add_readonly_single_input(&fpmd->control2);

  FN::GenericVectorArray vector_array{FN::CPPType_float3, 1};
  params_builder.add_vector_output(vector_array);

  FN::SceneTimeContext time_context;
  time_context.time = DEG_get_ctime(ctx->depsgraph);

  BKE::IDHandleLookup id_handle_lookup;
  FN::add_ids_used_by_nodes(id_handle_lookup, function_tree);

  BKE::IDDataCache id_data_cache;

  FN::MFContextBuilder context_builder;
  context_builder.add_global_context(id_handle_lookup);
  context_builder.add_global_context(time_context);
  context_builder.add_global_context(id_data_cache);

  function->call(BLI::IndexMask(1), params_builder, context_builder);

  ArrayRef<float3> output_points = vector_array[0].as_typed_ref<float3>();

  Mesh *mesh = BKE_mesh_new_nomain(output_points.size(), 0, 0, 0, 0);
  for (uint i = 0; i < output_points.size(); i++) {
    copy_v3_v3(mesh->mvert[i].co, output_points[i]);
  }

  return mesh;
}
