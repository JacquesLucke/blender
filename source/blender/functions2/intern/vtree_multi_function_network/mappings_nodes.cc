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

static Vector<bool> get_list_base_variadic_states(const VNode &vnode, StringRefNull prop_name)
{
  Vector<bool> list_states;
  RNA_BEGIN (vnode.rna(), itemptr, prop_name.data()) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      list_states.append(false);
    }
    else if (state == 1) {
      /* list case */
      list_states.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;
  return list_states;
}

static MFBuilderOutputSocket &build_pack_list_node(VTreeMFNetworkBuilder &builder,
                                                   const VNode &vnode,
                                                   const CPPType &base_type,
                                                   StringRefNull prop_name,
                                                   uint start_index)
{
  Vector<bool> list_states = get_list_base_variadic_states(vnode, prop_name);

  uint input_amount = list_states.size();
  uint output_param_index = (input_amount > 0 && list_states[0]) ? 0 : input_amount;

  const MultiFunction &fn = builder.allocate_function<FN::MF_PackList>(base_type, list_states);
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

template<typename T, T (*Compute)(T, T)>
static const MultiFunction &get_simple_math_function(VTreeMFNetworkBuilder &builder,
                                                     StringRef name,
                                                     ArrayRef<bool> list_states,
                                                     T default_value)
{
  if (list_states.size() == 0) {
    return builder.allocate_function<FN::MF_ConstantValue<T>>(default_value);
  }
  else {
    const MultiFunction &math_fn = builder.allocate_function<FN::MF_SimpleMath<T, Compute>>(
        name, list_states.size());

    if (list_states.contains(true)) {
      return builder.allocate_function<FN::MF_SimpleVectorize>(math_fn, list_states);
    }
    else {
      return math_fn;
    }
  }
}

template<typename T, T (*Compute)(T, T)>
static void insert_simple_math_function(VTreeMFNetworkBuilder &builder,
                                        const VNode &vnode,
                                        T default_value)
{
  Vector<bool> list_states = get_list_base_variadic_states(vnode, "variadic");
  const MultiFunction &fn = get_simple_math_function<T, Compute>(
      builder, vnode.name(), list_states, default_value);
  builder.add_function(
      fn, IndexRange(list_states.size()).as_array_ref(), {list_states.size()}, vnode);
}

template<typename T> T add_func_cb(T a, T b)
{
  return a + b;
}

template<typename T> T mul_func_cb(T a, T b)
{
  return a * b;
}

template<typename T> T min_func_cb(T a, T b)
{
  return std::min(a, b);
}

template<typename T> T max_func_cb(T a, T b)
{
  return std::max(a, b);
}

static void INSERT_add_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_simple_math_function<float, add_func_cb<float>>(builder, vnode, 0.0f);
}

static void INSERT_multiply_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_simple_math_function<float, mul_func_cb<float>>(builder, vnode, 1.0f);
}

static void INSERT_minimum_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_simple_math_function<float, min_func_cb<float>>(builder, vnode, 0.0f);
}

static void INSERT_maximum_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_simple_math_function<float, max_func_cb<float>>(builder, vnode, 0.0f);
}

template<typename T> T subtract_func_cb(T a, T b)
{
  return a - b;
}

template<typename T> T safe_divide_func_cb(T a, T b)
{
  return (b != 0) ? a / b : 0.0f;
}

template<typename T> T safe_power_func_cb(T a, T b)
{
  return (a >= 0) ? (T)std::pow(a, b) : (T)0;
}

template<typename T, T (*Compute)(T, T)>
void insert_two_inputs_math_function(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &base_fn = builder.allocate_function<MF_SimpleMath<T, Compute>>(vnode.name(),
                                                                                      2);
  const MultiFunction &fn = get_vectorized_function(
      builder, base_fn, vnode.rna(), {"use_list__a", "use_list__b"});
  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static void INSERT_subtract_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_two_inputs_math_function<float, subtract_func_cb<float>>(builder, vnode);
}

static void INSERT_divide_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_two_inputs_math_function<float, safe_divide_func_cb<float>>(builder, vnode);
}

static void INSERT_power_floats(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_two_inputs_math_function<float, safe_power_func_cb<float>>(builder, vnode);
}

template<typename T, T (*Compute)(const T &)>
static void insert_single_input_math_function(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  const MultiFunction &base_fn = builder.allocate_function<FN::MF_Mappping<T, T, Compute>>(
      vnode.name());
  const MultiFunction &fn = get_vectorized_function(builder, base_fn, vnode.rna(), {"use_list"});
  builder.add_function(fn, {0}, {1}, vnode);
}

template<typename T> T safe_sqrt_func_cb(const T &a)
{
  return (a >= 0.0) ? (T)std::sqrt(a) : 0.0f;
}

template<typename T> T abs_func_cb(const T &a)
{
  return (T)std::abs(a);
}

template<typename T> T sine_func_cb(const T &a)
{
  return (T)std::sin(a);
}

template<typename T> T cosine_func_cb(const T &a)
{
  return (T)std::cos(a);
}

static void INSERT_sqrt_float(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_single_input_math_function<float, safe_sqrt_func_cb<float>>(builder, vnode);
}

static void INSERT_abs_float(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_single_input_math_function<float, abs_func_cb<float>>(builder, vnode);
}

static void INSERT_sine(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_single_input_math_function<float, sine_func_cb<float>>(builder, vnode);
}

static void INSERT_cosine(VTreeMFNetworkBuilder &builder, const VNode &vnode)
{
  insert_single_input_math_function<float, cosine_func_cb<float>>(builder, vnode);
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

  mappings.vnode_inserters.add_new("fn_AddFloatsNode", INSERT_add_floats);
  mappings.vnode_inserters.add_new("fn_MultiplyFloatsNode", INSERT_multiply_floats);
  mappings.vnode_inserters.add_new("fn_MinimumFloatsNode", INSERT_minimum_floats);
  mappings.vnode_inserters.add_new("fn_MaximumFloatsNode", INSERT_maximum_floats);

  mappings.vnode_inserters.add_new("fn_SubtractFloatsNode", INSERT_subtract_floats);
  mappings.vnode_inserters.add_new("fn_DivideFloatsNode", INSERT_divide_floats);
  mappings.vnode_inserters.add_new("fn_PowerFloatsNode", INSERT_power_floats);

  mappings.vnode_inserters.add_new("fn_SqrtFloatNode", INSERT_sqrt_float);
  mappings.vnode_inserters.add_new("fn_AbsoluteFloatNode", INSERT_abs_float);
  mappings.vnode_inserters.add_new("fn_SineNode", INSERT_sine);
  mappings.vnode_inserters.add_new("fn_CosineNode", INSERT_cosine);
}

};  // namespace FN
