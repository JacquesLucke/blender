#include "mappings.h"
#include "builder.h"

#include "FN_multi_functions.h"

namespace FN {

static void INSERT_vector_math(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_AddFloat3s>();
  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static const MultiFunction &get_vectorized_function(
    VTreeMFNetworkBuilder &builder,
    const MultiFunction &base_function,
    PointerRNA *rna,
    ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(rna, prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    input_is_vectorized.append(is_vectorized);
  }

  if (input_is_vectorized.contains(true)) {
    return builder.allocate_function<FN::MF_SimpleVectorize>(base_function, input_is_vectorized);
  }
  else {
    return base_function;
  }
}

static void INSERT_float_math(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &base_fn = builder.allocate_function<FN::MF_AddFloats>();
  const MultiFunction &fn = get_vectorized_function(
      builder, base_fn, vnode.rna(), {"use_list__a", "use_list__b"});

  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static void INSERT_combine_vector(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &base_fn = builder.allocate_function<FN::MF_CombineVector>();
  const MultiFunction &fn = get_vectorized_function(
      builder, base_fn, vnode.rna(), {"use_list__x", "use_list__y", "use_list__z"});
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static void INSERT_separate_vector(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &base_fn = builder.allocate_function<FN::MF_SeparateVector>();
  const MultiFunction &fn = get_vectorized_function(
      builder, base_fn, vnode.rna(), {"use_list__vector"});
  builder.add_function(fn, {0}, {1, 2, 3}, vnode);
}

static void INSERT_list_length(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const CPPType &type = builder.cpp_type_from_property(vnode, "active_type");
  const MultiFunction &fn = builder.allocate_function<FN::MF_ListLength>(type);
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_get_list_element(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const CPPType &type = builder.cpp_type_from_property(vnode, "active_type");
  const MultiFunction &fn = builder.allocate_function<FN::MF_GetListElement>(type);
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static MFBuilderOutputSocket &build_pack_list_node(VTreeMFNetworkBuilder &builder,
                                                   const VNode &vnode,
                                                   const CPPType &base_type,
                                                   const char *prop_name,
                                                   uint start_index)
{
  Vector<bool> input_is_list;
  RNA_BEGIN (vnode.rna(), itemptr, prop_name) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      input_is_list.append(false);
    }
    else if (state == 1) {
      /* list case */
      input_is_list.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;

  uint input_amount = input_is_list.size();
  uint output_param_index = (input_amount > 0 && input_is_list[0]) ? 0 : input_amount;

  const MultiFunction &fn = builder.allocate_function<FN::MF_PackList>(base_type, input_is_list);
  MFBuilderFunctionNode &node = builder.add_function(
      fn, IndexRange(input_amount).as_array_ref(), {output_param_index});

  for (uint i = 0; i < input_amount; i++) {
    builder.map_sockets(vnode.input(start_index + i), *node.inputs()[i]);
  }

  return *node.outputs()[0];
}

static void INSERT_pack_list(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const CPPType &type = builder.cpp_type_from_property(vnode, "active_type");
  MFBuilderOutputSocket &packed_list_socket = build_pack_list_node(
      builder, vnode, type, "variadic", 0);
  builder.map_sockets(vnode.output(0), packed_list_socket);
}

static void INSERT_object_location(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_ObjectWorldLocation>();
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_text_length(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_TextLength>();
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_vertex_info(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_ContextVertexPosition>();
  builder.add_function(fn, {}, {0}, vnode);
}

static void INSERT_float_range(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_FloatRange>();
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static void INSERT_time_info(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &fn = builder.allocate_function<FN::MF_ContextCurrentFrame>();
  builder.add_function(fn, {}, {0}, vnode);
}

void add_vtree_node_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  mappings.vnode_inserters.add_new("fn_FloatMathNode", INSERT_float_math);
  mappings.vnode_inserters.add_new("fn_VectorMathNode", INSERT_vector_math);
  mappings.vnode_inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  mappings.vnode_inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  mappings.vnode_inserters.add_new("fn_ListLengthNode", INSERT_list_length);
  mappings.vnode_inserters.add_new("fn_PackListNode", INSERT_pack_list);
  mappings.vnode_inserters.add_new("fn_GetListElementNode", INSERT_get_list_element);
  mappings.vnode_inserters.add_new("fn_ObjectTransformsNode", INSERT_object_location);
  mappings.vnode_inserters.add_new("fn_TextLengthNode", INSERT_text_length);
  mappings.vnode_inserters.add_new("fn_VertexInfoNode", INSERT_vertex_info);
  mappings.vnode_inserters.add_new("fn_FloatRangeNode", INSERT_float_range);
  mappings.vnode_inserters.add_new("fn_TimeInfoNode", INSERT_time_info);
}

};  // namespace FN
