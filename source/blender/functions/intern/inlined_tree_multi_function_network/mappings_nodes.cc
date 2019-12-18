#include "mappings.h"
#include "builder.h"

#include "FN_multi_functions.h"
#include "FN_inlined_tree_multi_function_network_generation.h"

#include "BLI_math_cxx.h"

#include "BKE_surface_hook.h"

namespace FN {

using BLI::float3;
static void INSERT_combine_color(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_CombineColor>(
      {"use_list__red", "use_list__green", "use_list__blue", "use_list__alpha"});
}

static void INSERT_separate_color(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_SeparateColor>({"use_list__color"});
}

static void INSERT_combine_vector(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_CombineVector>(
      {"use_list__x", "use_list__y", "use_list__z"});
}

static void INSERT_separate_vector(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_SeparateVector>({"use_list__vector"});
}

static void INSERT_vector_from_value(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_VectorFromValue>({"use_list__value"});
}

static void INSERT_list_length(VNodeMFNetworkBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  builder.set_constructed_matching_fn<MF_ListLength>(type);
}

static void INSERT_get_list_element(VNodeMFNetworkBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  builder.set_constructed_matching_fn<MF_GetListElement>(type);
}

static void INSERT_get_list_elements(VNodeMFNetworkBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  builder.set_constructed_matching_fn<MF_GetListElements>(type);
}

static void INSERT_pack_list(VNodeMFNetworkBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  Vector<bool> list_states = builder.get_list_base_variadic_states("variadic");
  builder.set_constructed_matching_fn<MF_PackList>(type, list_states);
}

static void INSERT_object_location(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ObjectWorldLocation>();
}

static void INSERT_object_mesh_info(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ObjectVertexPositions>();
}

static void INSERT_get_position_on_surface(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_GetPositionOnSurface>(
      {"use_list__surface_hook"});
}

static void INSERT_get_normal_on_surface(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_GetNormalOnSurface>(
      {"use_list__surface_hook"});
}

static void INSERT_get_weight_on_surface(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_GetWeightOnSurface>(
      {"use_list__surface_hook", "use_list__vertex_group_name"});
}

static void INSERT_get_image_color_on_surface(VNodeMFNetworkBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_GetImageColorOnSurface>(
      {"use_list__surface_hook", "use_list__image"});
}

static void INSERT_particle_is_in_group(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ParticleIsInGroup>();
}

static void INSERT_switch(VNodeMFNetworkBuilder &builder)
{
  MFDataType type = builder.data_type_from_property("data_type");
  switch (type.category()) {
    case MFDataType::Single: {
      builder.set_constructed_matching_fn<MF_SwitchSingle>(type.single__cpp_type());
      break;
    }
    case MFDataType::Vector: {
      builder.set_constructed_matching_fn<MF_SwitchVector>(type.vector__cpp_base_type());
      break;
    }
  }
}

static void INSERT_select(VNodeMFNetworkBuilder &builder)
{
  MFDataType type = builder.data_type_from_property("data_type");
  uint inputs = RNA_collection_length(builder.rna(), "input_items");
  switch (type.category()) {
    case MFDataType::Single: {
      builder.set_constructed_matching_fn<MF_SelectSingle>(type.single__cpp_type(), inputs);
      break;
    }
    case MFDataType::Vector: {
      builder.set_constructed_matching_fn<MF_SelectVector>(type.vector__cpp_base_type(), inputs);
      break;
    }
  }
}

static void INSERT_text_length(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_TextLength>();
}

static void INSERT_vertex_info(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ContextVertexPosition>();
}

static void INSERT_float_range(VNodeMFNetworkBuilder &builder)
{
  int mode = RNA_enum_get(builder.rna(), "mode");
  switch (mode) {
    case 0: {
      builder.set_constructed_matching_fn<MF_FloatRange_Amount_Start_Step>();
      break;
    }
    case 1: {
      builder.set_constructed_matching_fn<MF_FloatRange_Amount_Start_Stop>();
      break;
    }
    default:
      BLI_assert(false);
  }
}

static void INSERT_time_info(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ContextCurrentFrame>();
}

template<typename InT, typename OutT, typename FuncT>
static std::function<void(MFMask mask, VirtualListRef<InT>, MutableArrayRef<OutT>)>
vectorize_function_1in_1out(FuncT func)
{
  return [=](MFMask mask, VirtualListRef<InT> inputs, MutableArrayRef<OutT> outputs) {
    if (inputs.is_non_single_full_array()) {
      ArrayRef<InT> in_array = inputs.as_full_array();
      mask.foreach_index([=](uint i) { outputs[i] = func(in_array[i]); });
    }
    else if (inputs.is_single_element()) {
      InT in_single = inputs.as_single_element();
      outputs.fill_indices(mask.indices(), in_single);
    }
    else {
      mask.foreach_index([=](uint i) { outputs[i] = func(inputs[i]); });
    }
  };
}

template<typename InT, typename OutT, typename FuncT>
static void build_math_fn_1in_1out(VNodeMFNetworkBuilder &builder, FuncT func)
{
  auto fn = vectorize_function_1in_1out<InT, OutT>(func);

  builder.set_vectorized_constructed_matching_fn<MF_Custom_In1_Out1<InT, OutT>>(
      {"use_list"}, builder.xnode().name(), fn);
}

template<typename InT1, typename InT2, typename OutT, typename FuncT>
static std::function<
    void(MFMask, VirtualListRef<InT1>, VirtualListRef<InT2>, MutableArrayRef<OutT>)>
vectorize_function_2in_1out(FuncT func)
{
  return [=](MFMask mask,
             VirtualListRef<InT1> inputs1,
             VirtualListRef<InT2> inputs2,
             MutableArrayRef<OutT> outputs) -> void {
    if (inputs1.is_non_single_full_array() && inputs2.is_non_single_full_array()) {
      ArrayRef<InT1> in1_array = inputs1.as_full_array();
      ArrayRef<InT2> in2_array = inputs2.as_full_array();
      mask.foreach_index(
          [=](uint i) { new (&outputs[i]) OutT(func(in1_array[i], in2_array[i])); });
    }
    else if (inputs1.is_non_single_full_array() && inputs2.is_single_element()) {
      ArrayRef<InT1> in1_array = inputs1.as_full_array();
      InT2 in2_single = inputs2.as_single_element();
      mask.foreach_index([=](uint i) { new (&outputs[i]) OutT(func(in1_array[i], in2_single)); });
    }
    else if (inputs1.is_single_element() && inputs2.is_non_single_full_array()) {
      InT1 in1_single = inputs1.as_single_element();
      ArrayRef<InT2> in2_array = inputs2.as_full_array();
      mask.foreach_index([=](uint i) { new (&outputs[i]) OutT(func(in1_single, in2_array[i])); });
    }
    else if (inputs1.is_single_element() && inputs2.is_single_element()) {
      InT1 in1_single = inputs1.as_single_element();
      InT2 in2_single = inputs2.as_single_element();
      OutT out_single = func(in1_single, in2_single);
      outputs.fill_indices(mask.indices(), out_single);
    }
    else {
      mask.foreach_index([=](uint i) { new (&outputs[i]) OutT(func(inputs1[i], inputs2[i])); });
    }
  };
}

template<typename InT1, typename InT2, typename OutT, typename FuncT>
static void build_math_fn_in2_out1(VNodeMFNetworkBuilder &builder, FuncT func)
{
  auto fn = vectorize_function_2in_1out<InT1, InT2, OutT>(func);
  builder.set_vectorized_constructed_matching_fn<MF_Custom_In2_Out1<InT1, InT2, OutT>>(
      {"use_list__a", "use_list__b"}, builder.xnode().name(), fn);
}

template<typename T, typename FuncT>
static void build_variadic_math_fn(VNodeMFNetworkBuilder &builder, FuncT func, T default_value)
{
  auto fn = vectorize_function_2in_1out<T, T, T>(func);

  Vector<bool> list_states = builder.get_list_base_variadic_states("variadic");
  if (list_states.size() == 0) {
    builder.set_constructed_matching_fn<MF_ConstantValue<T>>(default_value);
  }
  else {
    const MultiFunction &base_fn = builder.construct_fn<MF_VariadicMath<T>>(
        builder.xnode().name(), list_states.size(), fn);
    if (list_states.contains(true)) {
      builder.set_constructed_matching_fn<MF_SimpleVectorize>(base_fn, list_states);
    }
    else {
      builder.set_matching_fn(base_fn);
    }
  }
}

static void INSERT_add_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float a, float b) -> float { return a + b; }, 0.0f);
}

static void INSERT_multiply_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float a, float b) -> float { return a * b; }, 1.0f);
}

static void INSERT_minimum_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float a, float b) -> float { return std::min(a, b); }, 0.0f);
}

static void INSERT_maximum_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float a, float b) -> float { return std::max(a, b); }, 0.0f);
}

static void INSERT_subtract_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float, float, float>(builder,
                                              [](float a, float b) -> float { return a - b; });
}

static void INSERT_divide_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float, float, float>(
      builder, [](float a, float b) -> float { return (b != 0.0f) ? a / b : 0.0f; });
}

static void INSERT_power_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float, float, float>(builder, [](float a, float b) -> float {
    return (a >= 0.0f) ? (float)std::pow(a, b) : 0.0f;
  });
}

static void INSERT_sqrt_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_1in_1out<float, float>(
      builder, [](float a) -> float { return (a >= 0.0f) ? (float)std::sqrt(a) : 0.0f; });
}

static void INSERT_abs_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_1in_1out<float, float>(builder, [](float a) -> float { return std::abs(a); });
}

static void INSERT_sine_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_1in_1out<float, float>(builder, [](float a) -> float { return std::sin(a); });
}

static void INSERT_cosine_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_1in_1out<float, float>(builder, [](float a) -> float { return std::cos(a); });
}

static void INSERT_add_vectors(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float3 a, float3 b) -> float3 { return a + b; }, float3(0, 0, 0));
}

static void INSERT_multiply_vectors(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](float3 a, float3 b) -> float3 { return a * b; }, float3(1, 1, 1));
}

static void INSERT_subtract_vectors(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float3>(
      builder, [](float3 a, float3 b) -> float3 { return a - b; });
}

static void INSERT_divide_vectors(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float3>(builder, float3::safe_divide);
}

static void INSERT_vector_cross_product(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float3>(builder, float3::cross_high_precision);
}

static void INSERT_reflect_vector(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float3>(
      builder, [](float3 a, float3 b) { return a.reflected(b.normalized()); });
}

static void INSERT_project_vector(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float3>(builder, float3::project);
}

static void INSERT_vector_dot_product(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float>(builder, float3::dot);
}

static void INSERT_vector_distance(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float3, float>(builder, float3::distance);
}

static void INSERT_multiply_vector_with_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float3, float, float3>(builder, [](float3 a, float b) { return a * b; });
}

static void INSERT_boolean_and(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](bool a, bool b) { return a && b; }, true);
}

static void INSERT_boolean_or(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(
      builder, [](bool a, bool b) { return a || b; }, false);
}

static void INSERT_boolean_not(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_1in_1out<bool, bool>(builder, [](bool a) -> bool { return !a; });
}

static void INSERT_compare(VNodeMFNetworkBuilder &builder)
{
  build_math_fn_in2_out1<float, float, bool>(builder,
                                             [](float a, float b) -> bool { return a < b; });
}

static void INSERT_perlin_noise(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_PerlinNoise>();
}

static void create_particle_info_nodes(VNodeMFNetworkBuilder &builder,
                                       StringRef name,
                                       const XOutputSocket &xsocket)
{
  InlinedTreeMFNetworkBuilder &network_builder = builder.network_builder();
  const CPPType &type = network_builder.try_get_data_type(xsocket)->single__cpp_type();

  const MultiFunction &name_fn = network_builder.construct_fn<MF_ConstantValue<std::string>>(name);
  const MultiFunction &attribute_fn = network_builder.construct_fn<MF_ParticleAttribute>(type);
  MFBuilderFunctionNode &name_node = network_builder.add_function(name_fn);
  MFBuilderFunctionNode &attribute_node = network_builder.add_function(attribute_fn);
  network_builder.add_link(name_node.output(0), attribute_node.input(0));
  network_builder.map_sockets(xsocket, attribute_node.output(0));
}

static void INSERT_particle_info(VNodeMFNetworkBuilder &builder)
{
  const XNode &xnode = builder.xnode();

  create_particle_info_nodes(builder, "ID", xnode.output(0));
  create_particle_info_nodes(builder, "Position", xnode.output(1));
  create_particle_info_nodes(builder, "Velocity", xnode.output(2));
  create_particle_info_nodes(builder, "Birth Time", xnode.output(3));
}

static void INSERT_get_particle_attribute(VNodeMFNetworkBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("attribute_type");
  builder.set_constructed_matching_fn<MF_ParticleAttribute>(type);
}

static void INSERT_closest_surface_hook_on_object(VNodeMFNetworkBuilder &builder)
{
  const MultiFunction &main_fn = builder.construct_fn<MF_ClosestSurfaceHookOnObject>();
  const MultiFunction &position_fn = builder.construct_fn<MF_GetPositionOnSurface>();
  const MultiFunction &normal_fn = builder.construct_fn<MF_GetNormalOnSurface>();

  const MultiFunction &vectorized_main_fn = builder.get_vectorized_function(
      main_fn, {"use_list__object", "use_list__position"});

  InlinedTreeMFNetworkBuilder &network_builder = builder.network_builder();

  MFBuilderFunctionNode *main_node, *position_node, *normal_node;

  if (&main_fn == &vectorized_main_fn) {
    main_node = &network_builder.add_function(main_fn);
    position_node = &network_builder.add_function(position_fn);
    normal_node = &network_builder.add_function(normal_fn);
  }
  else {
    std::array<bool, 1> input_is_vectorized = {true};
    const MultiFunction &vectorized_position_fn = builder.construct_fn<MF_SimpleVectorize>(
        position_fn, input_is_vectorized);
    const MultiFunction &vectorized_normal_fn = builder.construct_fn<MF_SimpleVectorize>(
        normal_fn, input_is_vectorized);

    main_node = &network_builder.add_function(vectorized_main_fn);
    position_node = &network_builder.add_function(vectorized_position_fn);
    normal_node = &network_builder.add_function(vectorized_normal_fn);
  }

  network_builder.add_link(main_node->output(0), position_node->input(0));
  network_builder.add_link(main_node->output(0), normal_node->input(0));

  const XNode &xnode = builder.xnode();
  network_builder.map_sockets(xnode.inputs(), main_node->inputs());
  network_builder.map_sockets(xnode.output(0), main_node->output(0));
  network_builder.map_sockets(xnode.output(1), position_node->output(0));
  network_builder.map_sockets(xnode.output(2), normal_node->output(0));
}

static void INSERT_clamp_float(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_Clamp>(false);
}

static void INSERT_map_range(VNodeMFNetworkBuilder &builder)
{
  bool clamp = RNA_boolean_get(builder.rna(), "clamp");
  builder.set_constructed_matching_fn<MF_MapRange>(clamp);
}

static void INSERT_random_float(VNodeMFNetworkBuilder &builder)
{
  uint node_seed = (uint)RNA_int_get(builder.rna(), "node_seed");
  builder.set_constructed_matching_fn<MF_RandomFloat>(node_seed);
}

static void INSERT_random_floats(VNodeMFNetworkBuilder &builder)
{
  uint node_seed = (uint)RNA_int_get(builder.rna(), "node_seed");
  builder.set_constructed_matching_fn<MF_RandomFloats>(node_seed);
}

static void INSERT_random_vector(VNodeMFNetworkBuilder &builder)
{
  uint node_seed = (uint)RNA_int_get(builder.rna(), "node_seed");
  RandomVectorMode::Enum mode = (RandomVectorMode::Enum)RNA_enum_get(builder.rna(), "mode");
  builder.set_vectorized_constructed_matching_fn<MF_RandomVector>(
      {"use_list__factor", "use_list__seed"}, node_seed, mode);
}

static void INSERT_random_vectors(VNodeMFNetworkBuilder &builder)
{
  uint node_seed = (uint)RNA_int_get(builder.rna(), "node_seed");
  RandomVectorMode::Enum mode = (RandomVectorMode::Enum)RNA_enum_get(builder.rna(), "mode");
  builder.set_constructed_matching_fn<MF_RandomVectors>(node_seed, mode);
}

static void INSERT_value(VNodeMFNetworkBuilder &builder)
{
  const XOutputSocket &xsocket = builder.xnode().output(0);
  const VSocket &vsocket = xsocket.vsocket();
  InlinedTreeMFNetworkBuilder &network_builder = builder.network_builder();

  VSocketMFNetworkBuilder socket_builder{network_builder, vsocket};
  auto &inserter = network_builder.vtree_multi_function_mappings().xsocket_inserters.lookup(
      vsocket.idname());
  inserter(socket_builder);
  MFBuilderOutputSocket &built_socket = socket_builder.built_socket();

  network_builder.map_sockets(xsocket, built_socket);
}

static void INSERT_emitter_time_info(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_EmitterTimeInfo>();
}

static void INSERT_sample_object_surface(VNodeMFNetworkBuilder &builder)
{
  int value = RNA_enum_get(builder.rna(), "weight_mode");
  builder.set_constructed_matching_fn<MF_SampleObjectSurface>(value == 1);
}

static void INSERT_FindNonClosePointsNode(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_FindNonClosePoints>();
}

void add_inlined_tree_node_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  mappings.xnode_inserters.add_new("fn_CombineColorNode", INSERT_combine_color);
  mappings.xnode_inserters.add_new("fn_SeparateColorNode", INSERT_separate_color);
  mappings.xnode_inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  mappings.xnode_inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  mappings.xnode_inserters.add_new("fn_VectorFromValueNode", INSERT_vector_from_value);
  mappings.xnode_inserters.add_new("fn_SwitchNode", INSERT_switch);
  mappings.xnode_inserters.add_new("fn_SelectNode", INSERT_select);
  mappings.xnode_inserters.add_new("fn_ListLengthNode", INSERT_list_length);
  mappings.xnode_inserters.add_new("fn_PackListNode", INSERT_pack_list);
  mappings.xnode_inserters.add_new("fn_GetListElementNode", INSERT_get_list_element);
  mappings.xnode_inserters.add_new("fn_GetListElementsNode", INSERT_get_list_elements);
  mappings.xnode_inserters.add_new("fn_ObjectTransformsNode", INSERT_object_location);
  mappings.xnode_inserters.add_new("fn_ObjectMeshNode", INSERT_object_mesh_info);
  mappings.xnode_inserters.add_new("fn_GetPositionOnSurfaceNode", INSERT_get_position_on_surface);
  mappings.xnode_inserters.add_new("fn_GetNormalOnSurfaceNode", INSERT_get_normal_on_surface);
  mappings.xnode_inserters.add_new("fn_GetWeightOnSurfaceNode", INSERT_get_weight_on_surface);
  mappings.xnode_inserters.add_new("fn_GetImageColorOnSurfaceNode",
                                   INSERT_get_image_color_on_surface);
  mappings.xnode_inserters.add_new("fn_IsInGroupNode", INSERT_particle_is_in_group);
  mappings.xnode_inserters.add_new("fn_TextLengthNode", INSERT_text_length);
  mappings.xnode_inserters.add_new("fn_VertexInfoNode", INSERT_vertex_info);
  mappings.xnode_inserters.add_new("fn_FloatRangeNode", INSERT_float_range);
  mappings.xnode_inserters.add_new("fn_TimeInfoNode", INSERT_time_info);
  mappings.xnode_inserters.add_new("fn_CompareNode", INSERT_compare);
  mappings.xnode_inserters.add_new("fn_PerlinNoiseNode", INSERT_perlin_noise);
  mappings.xnode_inserters.add_new("fn_ParticleInfoNode", INSERT_particle_info);
  mappings.xnode_inserters.add_new("fn_GetParticleAttributeNode", INSERT_get_particle_attribute);
  mappings.xnode_inserters.add_new("fn_ClosestLocationOnObjectNode",
                                   INSERT_closest_surface_hook_on_object);
  mappings.xnode_inserters.add_new("fn_MapRangeNode", INSERT_map_range);
  mappings.xnode_inserters.add_new("fn_FloatClampNode", INSERT_clamp_float);
  mappings.xnode_inserters.add_new("fn_RandomFloatNode", INSERT_random_float);
  mappings.xnode_inserters.add_new("fn_RandomFloatsNode", INSERT_random_floats);
  mappings.xnode_inserters.add_new("fn_RandomVectorNode", INSERT_random_vector);
  mappings.xnode_inserters.add_new("fn_RandomVectorsNode", INSERT_random_vectors);
  mappings.xnode_inserters.add_new("fn_ValueNode", INSERT_value);
  mappings.xnode_inserters.add_new("fn_EmitterTimeInfoNode", INSERT_emitter_time_info);
  mappings.xnode_inserters.add_new("fn_SampleObjectSurfaceNode", INSERT_sample_object_surface);
  mappings.xnode_inserters.add_new("fn_FindNonClosePointsNode", INSERT_FindNonClosePointsNode);

  mappings.xnode_inserters.add_new("fn_AddFloatsNode", INSERT_add_floats);
  mappings.xnode_inserters.add_new("fn_MultiplyFloatsNode", INSERT_multiply_floats);
  mappings.xnode_inserters.add_new("fn_MinimumFloatsNode", INSERT_minimum_floats);
  mappings.xnode_inserters.add_new("fn_MaximumFloatsNode", INSERT_maximum_floats);

  mappings.xnode_inserters.add_new("fn_SubtractFloatsNode", INSERT_subtract_floats);
  mappings.xnode_inserters.add_new("fn_DivideFloatsNode", INSERT_divide_floats);
  mappings.xnode_inserters.add_new("fn_PowerFloatsNode", INSERT_power_floats);

  mappings.xnode_inserters.add_new("fn_SqrtFloatNode", INSERT_sqrt_float);
  mappings.xnode_inserters.add_new("fn_AbsoluteFloatNode", INSERT_abs_float);
  mappings.xnode_inserters.add_new("fn_SineFloatNode", INSERT_sine_float);
  mappings.xnode_inserters.add_new("fn_CosineFloatNode", INSERT_cosine_float);

  mappings.xnode_inserters.add_new("fn_AddVectorsNode", INSERT_add_vectors);
  mappings.xnode_inserters.add_new("fn_SubtractVectorsNode", INSERT_subtract_vectors);
  mappings.xnode_inserters.add_new("fn_MultiplyVectorsNode", INSERT_multiply_vectors);
  mappings.xnode_inserters.add_new("fn_DivideVectorsNode", INSERT_divide_vectors);

  mappings.xnode_inserters.add_new("fn_VectorCrossProductNode", INSERT_vector_cross_product);
  mappings.xnode_inserters.add_new("fn_ReflectVectorNode", INSERT_reflect_vector);
  mappings.xnode_inserters.add_new("fn_ProjectVectorNode", INSERT_project_vector);
  mappings.xnode_inserters.add_new("fn_VectorDotProductNode", INSERT_vector_dot_product);
  mappings.xnode_inserters.add_new("fn_VectorDistanceNode", INSERT_vector_distance);
  mappings.xnode_inserters.add_new("fn_MultiplyVectorWithFloatNode",
                                   INSERT_multiply_vector_with_float);

  mappings.xnode_inserters.add_new("fn_BooleanAndNode", INSERT_boolean_and);
  mappings.xnode_inserters.add_new("fn_BooleanOrNode", INSERT_boolean_or);
  mappings.xnode_inserters.add_new("fn_BooleanNotNode", INSERT_boolean_not);
}

};  // namespace FN
