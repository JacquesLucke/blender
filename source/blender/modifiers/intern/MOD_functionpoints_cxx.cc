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
#include "FN_multi_function_expression.h"

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
    FN::VariableExprNode var_a{"a", FN::MFDataType::ForSingle<float>()};
    FN::MF_Convert<float, int> convert_fn;
    FN::FunctionExprNode convert_expr{convert_fn, 1, {&var_a}};
    FN::MF_Custom_In1_Out1<int, int> math_fn{"My Operation", [](int a) { return a + 42; }};
    FN::FunctionExprNode math_expr{math_fn, 1, {&convert_expr}};

    FN::MFNetworkBuilder network_builder;
    FN::MFBuilderOutputSocket &value_a_socket =
        network_builder
            .add_dummy("Input 'a'", {}, {FN::MFDataType::ForSingle<float>()}, {}, {"Value"})
            .output(0);
    BLI::StringMap<FN::MFBuilderOutputSocket *> variable_map;
    variable_map.add_new("a", &value_a_socket);

    FN::MFBuilderOutputSocket &expr_output = math_expr.build_network(network_builder,
                                                                     variable_map);
    FN::MFBuilderDummyNode &output_node = network_builder.add_dummy(
        "Output", {expr_output.data_type()}, {}, {"Value"}, {});
    network_builder.add_link(expr_output, output_node.input(0));

    uint index_of_input_node = network_builder.current_index_of(value_a_socket.node().as_dummy());
    uint index_of_output_node = network_builder.current_index_of(output_node);

    FN::MFNetwork network{network_builder};
    FN::MF_EvaluateNetwork network_fn({&network.dummy_nodes()[index_of_input_node]->output(0)},
                                      {&network.dummy_nodes()[index_of_output_node]->input(0)});

    Vector<float> input_values = {5.4f, 6.0f, 8.0f};
    BLI::Array<int> output_values(3, 0);

    FN::MFParamsBuilder params_builder{network_fn, input_values.size()};
    params_builder.add_readonly_single_input(input_values.as_ref());
    params_builder.add_single_output(output_values.as_mutable_ref());

    FN::MFContextBuilder context_builder;

    network_fn.call(IndexRange(input_values.size()), params_builder, context_builder);

    output_values.as_ref().print_as_lines("Output", [](int value) { std::cout << value; });

    network_builder.to_dot__clipboard();
  }

  return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  if (fpmd->function_tree == nullptr) {
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  FN::BTreeVTreeMap vtrees;
  FunctionTree function_tree(btree, vtrees);

  BLI::ResourceCollector resources;
  auto function = FN::MFGeneration::generate_node_tree_multi_function(function_tree, resources);

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
}
