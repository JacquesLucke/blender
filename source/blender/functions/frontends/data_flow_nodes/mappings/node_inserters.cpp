#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_data_flow_nodes.hpp"

#include "RNA_access.h"

#include "DNA_node_types.h"

#include "registry.hpp"

namespace FN {
namespace DataFlowNodes {

using namespace Types;

struct AutoVectorizedInput {
  const char *prop_name;
  Function &default_value_builder;
};

static Function &get_vectorized_function(Function &original_fn,
                                         PointerRNA &node_rna,
                                         ArrayRef<AutoVectorizedInput> auto_vectorized_inputs)
{
#ifdef DEBUG
  BLI_assert(original_fn.input_amount() == auto_vectorized_inputs.size());
  for (uint i = 0; i < original_fn.input_amount(); i++) {
    BLI_assert(original_fn.input_type(i) ==
               auto_vectorized_inputs[i].default_value_builder.output_type(0));
  }
#endif

  Vector<bool> vectorized_inputs;
  Vector<Function *> used_default_value_builders;
  for (auto &input : auto_vectorized_inputs) {
    char state[5];
    BLI_assert(RNA_string_length(&node_rna, input.prop_name) == strlen("BASE"));
    RNA_string_get(&node_rna, input.prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    vectorized_inputs.append(is_vectorized);
    if (is_vectorized) {
      used_default_value_builders.append(&input.default_value_builder);
    }
  }

  if (vectorized_inputs.contains(true)) {
    return Functions::to_vectorized_function__with_cache(
        original_fn, vectorized_inputs, used_default_value_builders);
  }
  else {
    return original_fn;
  }
}

static void INSERT_object_transforms(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Function &fn = Functions::GET_FN_object_location();
  builder.insert_matching_function(fn, vnode);
}

static Function &get_float_math_function(int operation)
{
  switch (operation) {
    case 1:
      return Functions::GET_FN_add_floats();
    case 2:
      return Functions::GET_FN_sub_floats();
    case 3:
      return Functions::GET_FN_multiply_floats();
    case 4:
      return Functions::GET_FN_divide_floats();
    case 5:
      return Functions::GET_FN_power_floats();
    case 6:
      return Functions::GET_FN_log_floats();
    case 7:
      return Functions::GET_FN_sqrt_float();
    case 8:
      return Functions::GET_FN_abs_float();
    case 9:
      return Functions::GET_FN_min_floats();
    case 10:
      return Functions::GET_FN_max_floats();
    case 11:
      return Functions::GET_FN_sin_float();
    case 12:
      return Functions::GET_FN_cos_float();
    case 13:
      return Functions::GET_FN_tan_float();
    case 14:
      return Functions::GET_FN_asin_float();
    case 15:
      return Functions::GET_FN_acos_float();
    case 16:
      return Functions::GET_FN_atan_float();
    case 17:
      return Functions::GET_FN_atan2_floats();
    case 18:
      return Functions::GET_FN_mod_floats();
    case 19:
      return Functions::GET_FN_fract_float();
    case 20:
      return Functions::GET_FN_ceil_float();
    case 21:
      return Functions::GET_FN_floor_float();
    case 22:
      return Functions::GET_FN_round_float();
    case 23:
      return Functions::GET_FN_snap_floats();
    default:
      BLI_assert(false);
      return Functions::GET_FN_none();
  }
}

static void INSERT_float_math(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  int operation = RNA_enum_get(&rna, "operation");

  Function &original_fn = get_float_math_function(operation);
  uint input_amount = original_fn.input_amount();

  if (input_amount == 1) {
    Function &fn = get_vectorized_function(
        original_fn, rna, {{"use_list__a", Functions::GET_FN_output_float_0()}});
    builder.insert_matching_function(fn, vnode);
  }
  else {
    BLI_assert(input_amount == 2);
    Function &fn = get_vectorized_function(original_fn,
                                           rna,
                                           {{"use_list__a", Functions::GET_FN_output_float_0()},
                                            {"use_list__b", Functions::GET_FN_output_float_0()}});
    builder.insert_matching_function(fn, vnode);
  }
}

static Function &get_vector_math_function(int operation)
{
  switch (operation) {
    case 1:
      return Functions::GET_FN_add_vectors();
    case 2:
      return Functions::GET_FN_sub_vectors();
    case 3:
      return Functions::GET_FN_mul_vectors();
    case 4:
      return Functions::GET_FN_div_vectors();
    case 5:
      return Functions::GET_FN_cross_vectors();
    case 6:
      return Functions::GET_FN_reflect_vector();
    case 7:
      return Functions::GET_FN_project_vector();
    case 8:
      return Functions::GET_FN_dot_product();
    default:
      BLI_assert(false);
      return Functions::GET_FN_none();
  }
}

static void INSERT_vector_math(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  int operation = RNA_enum_get(&rna, "operation");

  Function &fn = get_vectorized_function(get_vector_math_function(operation),
                                         rna,
                                         {{"use_list__a", Functions::GET_FN_output_float3_0()},
                                          {"use_list__b", Functions::GET_FN_output_float3_0()}});
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_clamp(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Function &max_fn = Functions::GET_FN_max_floats();
  Function &min_fn = Functions::GET_FN_min_floats();

  BuilderNode *max_node = builder.insert_function(max_fn, vnode);
  BuilderNode *min_node = builder.insert_function(min_fn, vnode);

  builder.insert_link(max_node->output(0), min_node->input(0));
  builder.map_input_socket(max_node->input(0), vnode->input(0));
  builder.map_input_socket(max_node->input(1), vnode->input(1));
  builder.map_input_socket(min_node->input(1), vnode->input(2));
  builder.map_output_socket(min_node->output(0), vnode->output(0));
}

static void INSERT_get_list_element(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Type *base_type = builder.query_type_property(vnode, "active_type");
  Function &fn = Functions::GET_FN_get_list_element(base_type);
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_list_length(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Type *base_type = builder.query_type_property(vnode, "active_type");
  Function &fn = Functions::GET_FN_list_length(base_type);
  builder.insert_matching_function(fn, vnode);
}

static BuilderOutputSocket *insert_pack_list_sockets(VTreeDataGraphBuilder &builder,
                                                     VirtualNode *vnode,
                                                     Type *base_type,
                                                     const char *prop_name,
                                                     uint start_index)
{
  auto &empty_fn = Functions::GET_FN_empty_list(base_type);
  BuilderNode *node = builder.insert_function(empty_fn, vnode);

  PointerRNA rna = vnode->rna();

  uint index = start_index;
  RNA_BEGIN (&rna, itemptr, prop_name) {
    BuilderNode *new_node;
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      auto &append_fn = Functions::GET_FN_append_to_list(base_type);
      new_node = builder.insert_function(append_fn, vnode);
      builder.insert_link(node->output(0), new_node->input(0));
      builder.map_input_socket(new_node->input(1), vnode->input(index));
    }
    else if (state == 1) {
      /* list case */
      auto &combine_fn = Functions::GET_FN_combine_lists(base_type);
      new_node = builder.insert_function(combine_fn, vnode);
      builder.insert_link(node->output(0), new_node->input(0));
      builder.map_input_socket(new_node->input(1), vnode->input(index));
    }
    else {
      BLI_assert(false);
      new_node = nullptr;
    }
    node = new_node;
    index++;
  }
  RNA_END;

  return node->output(0);
}

static void INSERT_pack_list(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Type *base_type = builder.query_type_property(vnode, "active_type");
  BuilderOutputSocket *packed_list_socket = insert_pack_list_sockets(
      builder, vnode, base_type, "variadic", 0);
  builder.map_output_socket(packed_list_socket, vnode->output(0));
}

static void INSERT_call(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();

  PointerRNA btree_ptr = RNA_pointer_get(&rna, "function_tree");
  bNodeTree *btree = (bNodeTree *)btree_ptr.owner_id;

  if (btree == nullptr) {
    BLI_assert(vnode->inputs().size() == 0);
    BLI_assert(vnode->outputs().size() == 0);
    return;
  }

  Optional<std::unique_ptr<Function>> optional_fn = generate_function(btree);
  BLI_assert(optional_fn.has_value());
  auto fn = optional_fn.extract();
  builder.insert_matching_function(*fn, vnode);
  builder.add_resource(std::move(fn), "Generated function in for Call node");
}

static void INSERT_switch(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  Type *data_type = builder.query_type_property(vnode, "data_type");
  Function &fn = Functions::GET_FN_bool_switch(data_type);
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_combine_vector(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Function &fn = get_vectorized_function(Functions::GET_FN_combine_vector(),
                                         rna,
                                         {{"use_list__x", Functions::GET_FN_output_float_0()},
                                          {"use_list__y", Functions::GET_FN_output_float_0()},
                                          {"use_list__z", Functions::GET_FN_output_float_0()}});
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_separate_vector(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Function &fn = get_vectorized_function(
      Functions::GET_FN_separate_vector(),
      rna,
      {{"use_list__vector", Functions::GET_FN_output_float3_0()}});
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_separate_color(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Function &fn = get_vectorized_function(
      Functions::GET_FN_separate_color(),
      rna,
      {{"use_list__color", Functions::GET_FN_output_magenta()}});
  builder.insert_matching_function(fn, vnode);
}

static void INSERT_combine_color(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Function &fn = get_vectorized_function(
      Functions::GET_FN_combine_color(),
      rna,
      {{"use_list__red", Functions::GET_FN_output_float_0()},
       {"use_list__green", Functions::GET_FN_output_float_0()},
       {"use_list__blue", Functions::GET_FN_output_float_0()},
       {"use_list__alpha", Functions::GET_FN_output_float_1()}});
  builder.insert_matching_function(fn, vnode);
}

static Function &get_compare_function(int operation)
{
  switch (operation) {
    case 1:
      return Functions::GET_FN_less_than_float();
    default:
      BLI_assert(false);
      return Functions::GET_FN_none();
  }
}

static void INSERT_compare(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  int operation = RNA_enum_get(&rna, "operation");
  Function &fn = get_vectorized_function(get_compare_function(operation),
                                         rna,
                                         {{"use_list__a", Functions::GET_FN_output_float_0()},
                                          {"use_list__b", Functions::GET_FN_output_float_0()}});
  builder.insert_matching_function(fn, vnode);
}

static Function &get_boolean_math_function(int operation)
{
  switch (operation) {
    case 1:
      return Functions::GET_FN_and();
    case 2:
      return Functions::GET_FN_or();
    case 3:
      return Functions::GET_FN_not();
    default:
      BLI_assert(false);
      return Functions::GET_FN_none();
  }
}

static void INSERT_boolean_math(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  int operation = RNA_enum_get(&rna, "operation");
  Function &original_fn = get_boolean_math_function(operation);
  uint input_amount = original_fn.input_amount();
  if (input_amount == 1) {
    Function &fn = get_vectorized_function(
        original_fn, rna, {{"use_list__a", Functions::GET_FN_output_false()}});
    builder.insert_matching_function(fn, vnode);
  }
  else {
    BLI_assert(input_amount == 2);
    Function &fn = get_vectorized_function(original_fn,
                                           rna,
                                           {{"use_list__a", Functions::GET_FN_output_false()},
                                            {"use_list__b", Functions::GET_FN_output_true()}});
    builder.insert_matching_function(fn, vnode);
  }
}

void REGISTER_node_inserters(std::unique_ptr<NodeInserters> &inserters)
{
#define REGISTER_FUNCTION(idname, fn) inserters->register_function(idname, Functions::GET_FN_##fn)
#define REGISTER_INSERTER(idname, fn) inserters->register_inserter(idname, fn)

  REGISTER_FUNCTION("fn_FloatRangeNode", float_range);
  REGISTER_FUNCTION("fn_MapRangeNode", map_range);
  REGISTER_FUNCTION("fn_ObjectMeshNode", object_mesh_vertices);
  REGISTER_FUNCTION("fn_RandomNumberNode", random_number);
  REGISTER_FUNCTION("fn_VectorDistanceNode", vector_distance);
  REGISTER_FUNCTION("fn_TextLengthNode", string_length);

  REGISTER_INSERTER("fn_CallNode", INSERT_call);
  REGISTER_INSERTER("fn_ClampNode", INSERT_clamp);
  REGISTER_INSERTER("fn_CombineColorNode", INSERT_combine_color);
  REGISTER_INSERTER("fn_CombineVectorNode", INSERT_combine_vector);
  REGISTER_INSERTER("fn_CompareNode", INSERT_compare);
  REGISTER_INSERTER("fn_FloatMathNode", INSERT_float_math);
  REGISTER_INSERTER("fn_GetListElementNode", INSERT_get_list_element);
  REGISTER_INSERTER("fn_ListLengthNode", INSERT_list_length);
  REGISTER_INSERTER("fn_ObjectTransformsNode", INSERT_object_transforms);
  REGISTER_INSERTER("fn_PackListNode", INSERT_pack_list);
  REGISTER_INSERTER("fn_SeparateColorNode", INSERT_separate_color);
  REGISTER_INSERTER("fn_SeparateVectorNode", INSERT_separate_vector);
  REGISTER_INSERTER("fn_SwitchNode", INSERT_switch);
  REGISTER_INSERTER("fn_VectorMathNode", INSERT_vector_math);
  REGISTER_INSERTER("fn_BooleanMathNode", INSERT_boolean_math);

#undef REGISTER_INSERTER
#undef REGISTER_FUNCTION
}

}  // namespace DataFlowNodes
}  // namespace FN
