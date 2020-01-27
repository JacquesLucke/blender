#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_id_data_cache.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_timeit.h"

#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

using BKE::VNode;
using BLI::Array;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::MutableArrayRef;
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

BLI_NOINLINE static void compute_triangle_areas(ArrayRef<float3> points_a,
                                                ArrayRef<float3> points_b,
                                                ArrayRef<float3> points_c,
                                                MutableArrayRef<float> r_areas)
{
  uint amount = points_a.size();
  BLI_assert(points_b.size() == amount);
  BLI_assert(points_c.size() == amount);
  BLI_assert(r_areas.size() == amount);

  for (uint i = 0; i < amount; i++) {
    float3 a = points_a[i];
    float3 b = points_b[i];
    float3 c = points_c[i];

    float side1 = float3::distance(a, b);
    float side2 = float3::distance(a, c);
    float side3 = float3::distance(b, c);

    float s = (side1 + side2 + side3) * 0.5f;
    float area = sqrtf(s * (s - side1) * (s - side2) * (s - side3));
    r_areas[i] = area;
  }
}

Mesh *MOD_functionpoints_do(FunctionPointsModifierData *fpmd,
                            const struct ModifierEvalContext *ctx)
{
  if (fpmd->function_tree == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  BLI::ResourceCollector resources;
  std::unique_ptr<const FN::MultiFunction> function;
  {
    SCOPED_TIMER("generate function from node tree")
    FN::BTreeVTreeMap vtrees;
    FunctionTree function_tree(btree, vtrees);
    function = FN::MFGeneration::generate_node_tree_multi_function(function_tree, resources);
  }

  MFParamsBuilder params_builder(*function, 1);
  FN::MFContextBuilder context_builder;

  uint amount = 10000000;
  Array<float3> points_a(amount);
  Array<float3> points_b(amount);
  Array<float3> points_c(amount);
  Array<float> results1(amount, 0.0f);
  Array<float> results2(amount, 0.0f);

  for (uint i : IndexRange(amount)) {
    points_a[i] = {1, 2, 3};
    points_b[i] = {5, 3, 2};
    points_c[i] = {3, 5, 7};
  }

  params_builder.add_readonly_single_input(points_a.as_ref());
  params_builder.add_readonly_single_input(points_b.as_ref());
  params_builder.add_readonly_single_input(points_c.as_ref());
  params_builder.add_single_output(results1.as_mutable_ref());

  {
    SCOPED_TIMER("Node Tree");
    function->call(IndexRange(amount), params_builder, context_builder);
  }
  std::cout << "Area 1: " << results1[0] << "\n";

  {
    SCOPED_TIMER("C++");
    compute_triangle_areas(points_a, points_b, points_c, results2);
  }
  std::cout << "Area 2: " << results2[0] << "\n";

  return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  /*

  MFParamsBuilder params_builder(*function, 1);
  params_builder.add_readonly_single_input(&fpmd->control1);
  params_builder.add_readonly_single_input(&fpmd->control2);

  FN::GenericVectorArray vector_array{FN::CPP_TYPE<float3>(), 1};
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
  */
}
