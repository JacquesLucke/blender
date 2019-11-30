#include "mappings.h"
#include "builder.h"

#include "FN_multi_functions.h"
#include "FN_inlined_tree_multi_function_network_generation.h"

#include "BLI_math_cxx.h"

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
  builder.set_constructed_matching_fn<MF_GetPositionOnSurface>();
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
  builder.set_constructed_matching_fn<MF_FloatRange>();
}

static void INSERT_time_info(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ContextCurrentFrame>();
}

template<typename InT, typename OutT>
static void build_math_fn(VNodeMFNetworkBuilder &builder, OutT (*func)(InT))
{
  auto fn =
      [func](MFMask mask, VirtualListRef<InT> inputs, MutableArrayRef<OutT> outputs) -> void {
    for (uint i : mask.indices()) {
      new (&outputs[i]) OutT(func(inputs[i]));
    }
  };

  builder.set_vectorized_constructed_matching_fn<MF_Custom_In1_Out1<InT, OutT>>(
      {"use_list"}, builder.xnode().name(), fn);
}

template<typename InT1, typename InT2, typename OutT>
static void build_math_fn(VNodeMFNetworkBuilder &builder, OutT (*func)(InT1, InT2))
{
  auto fn = [func](MFMask mask,
                   VirtualListRef<InT1> inputs1,
                   VirtualListRef<InT2> inputs2,
                   MutableArrayRef<OutT> outputs) -> void {
    for (uint i : mask.indices()) {
      new (&outputs[i]) OutT(func(inputs1[i], inputs2[i]));
    }
  };

  builder.set_vectorized_constructed_matching_fn<MF_Custom_In2_Out1<InT1, InT2, OutT>>(
      {"use_list__a", "use_list__b"}, builder.xnode().name(), fn);
}

template<typename T>
static void build_variadic_math_fn(VNodeMFNetworkBuilder &builder,
                                   T (*func)(T, T),
                                   T default_value)
{
  auto fn = [func](MFMask mask,
                   VirtualListRef<T> inputs1,
                   VirtualListRef<T> inputs2,
                   MutableArrayRef<T> outputs) {
    for (uint i : mask.indices()) {
      outputs[i] = func(inputs1[i], inputs2[i]);
    }
  };

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
  build_variadic_math_fn(builder, +[](float a, float b) -> float { return a + b; }, 0.0f);
}

static void INSERT_multiply_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float a, float b) -> float { return a * b; }, 1.0f);
}

static void INSERT_minimum_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float a, float b) -> float { return std::min(a, b); }, 0.0f);
}

static void INSERT_maximum_floats(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float a, float b) -> float { return std::max(a, b); }, 0.0f);
}

static void INSERT_subtract_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a, float b) -> float { return a - b; });
}

static void INSERT_divide_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a, float b) -> float { return (b != 0.0f) ? a / b : 0.0f; });
}

static void INSERT_power_floats(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a, float b) -> float {
    return (a >= 0.0f) ? (float)std::pow(a, b) : 0.0f;
  });
}

static void INSERT_sqrt_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder,
                +[](float a) -> float { return (a >= 0.0f) ? (float)std::sqrt(a) : 0.0f; });
}

static void INSERT_abs_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a) -> float { return std::abs(a); });
}

static void INSERT_sine_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a) -> float { return std::sin(a); });
}

static void INSERT_cosine_float(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a) -> float { return std::cos(a); });
}

static void INSERT_add_vectors(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float3 a, float3 b) -> float3 { return a + b; }, {0, 0, 0});
}

static void INSERT_multiply_vectors(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float3 a, float3 b) -> float3 { return a * b; }, {1, 1, 1});
}

static void INSERT_subtract_vectors(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float3 a, float3 b) -> float3 { return a - b; });
}

static void INSERT_divide_vectors(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, float3::safe_divide);
}

static void INSERT_vector_cross_product(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, float3::cross_high_precision);
}

static void INSERT_reflect_vector(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float3 a, float3 b) { return a.reflected(b.normalized()); });
}

static void INSERT_project_vector(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, float3::project);
}

static void INSERT_vector_dot_product(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, float3::dot);
}

static void INSERT_vector_distance(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, float3::distance);
}

static void INSERT_boolean_and(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](bool a, bool b) { return a && b; }, true);
}

static void INSERT_boolean_or(VNodeMFNetworkBuilder &builder)
{
  build_variadic_math_fn(builder, +[](bool a, bool b) { return a || b; }, false);
}

static void INSERT_boolean_not(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](bool a) -> bool { return !a; });
}

static void INSERT_compare(VNodeMFNetworkBuilder &builder)
{
  build_math_fn(builder, +[](float a, float b) -> bool { return a < b; });
}

static void INSERT_perlin_noise(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_PerlinNoise>();
}

static void INSERT_particle_info(VNodeMFNetworkBuilder &builder)
{
  VTreeMFNetworkBuilder &network_builder = builder.network_builder();
  const XNode &xnode = builder.xnode();

  {
    const MultiFunction &fn = network_builder.construct_fn<MF_ParticleAttributes>("ID",
                                                                                  CPP_TYPE<int>());
    MFBuilderFunctionNode &node = network_builder.add_function(fn);
    network_builder.map_sockets(xnode.output(0), node.output(0));
  }
  {
    const MultiFunction &fn = network_builder.construct_fn<MF_ParticleAttributes>(
        "Position", CPP_TYPE<float3>());
    MFBuilderFunctionNode &node = network_builder.add_function(fn);
    network_builder.map_sockets(xnode.output(1), node.output(0));
  }
  {
    const MultiFunction &fn = network_builder.construct_fn<MF_ParticleAttributes>(
        "Velocity", CPP_TYPE<float3>());
    MFBuilderFunctionNode &node = network_builder.add_function(fn);
    network_builder.map_sockets(xnode.output(2), node.output(0));
  }
  {
    const MultiFunction &fn = network_builder.construct_fn<MF_ParticleAttributes>(
        "Birth Time", CPP_TYPE<float>());
    MFBuilderFunctionNode &node = network_builder.add_function(fn);
    network_builder.map_sockets(xnode.output(3), node.output(0));
  }
}

static void INSERT_closest_location_on_object(VNodeMFNetworkBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ClosestLocationOnObject>();
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
  builder.set_constructed_matching_fn<MF_RandomFloat>();
}

void add_inlined_tree_node_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  mappings.xnode_inserters.add_new("fn_CombineColorNode", INSERT_combine_color);
  mappings.xnode_inserters.add_new("fn_SeparateColorNode", INSERT_separate_color);
  mappings.xnode_inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  mappings.xnode_inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  mappings.xnode_inserters.add_new("fn_SwitchNode", INSERT_switch);
  mappings.xnode_inserters.add_new("fn_ListLengthNode", INSERT_list_length);
  mappings.xnode_inserters.add_new("fn_PackListNode", INSERT_pack_list);
  mappings.xnode_inserters.add_new("fn_GetListElementNode", INSERT_get_list_element);
  mappings.xnode_inserters.add_new("fn_ObjectTransformsNode", INSERT_object_location);
  mappings.xnode_inserters.add_new("fn_ObjectMeshNode", INSERT_object_mesh_info);
  mappings.xnode_inserters.add_new("fn_GetPositionOnSurfaceNode", INSERT_get_position_on_surface);
  mappings.xnode_inserters.add_new("fn_TextLengthNode", INSERT_text_length);
  mappings.xnode_inserters.add_new("fn_VertexInfoNode", INSERT_vertex_info);
  mappings.xnode_inserters.add_new("fn_FloatRangeNode", INSERT_float_range);
  mappings.xnode_inserters.add_new("fn_TimeInfoNode", INSERT_time_info);
  mappings.xnode_inserters.add_new("fn_CompareNode", INSERT_compare);
  mappings.xnode_inserters.add_new("fn_PerlinNoiseNode", INSERT_perlin_noise);
  mappings.xnode_inserters.add_new("fn_ParticleInfoNode", INSERT_particle_info);
  mappings.xnode_inserters.add_new("fn_ClosestLocationOnObjectNode",
                                   INSERT_closest_location_on_object);
  mappings.xnode_inserters.add_new("fn_MapRangeNode", INSERT_map_range);
  mappings.xnode_inserters.add_new("fn_FloatClampNode", INSERT_clamp_float);
  mappings.xnode_inserters.add_new("fn_RandomFloatNode", INSERT_random_float);

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

  mappings.xnode_inserters.add_new("fn_BooleanAndNode", INSERT_boolean_and);
  mappings.xnode_inserters.add_new("fn_BooleanOrNode", INSERT_boolean_or);
  mappings.xnode_inserters.add_new("fn_BooleanNotNode", INSERT_boolean_not);
}

};  // namespace FN
