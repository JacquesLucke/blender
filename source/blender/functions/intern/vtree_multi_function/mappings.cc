#include "BLI_lazy_init_cxx.h"
#include "BLI_math_cxx.h"

#include "RNA_access.h"

#include "mappings.h"

#include "vsocket_multi_function_builder.h"
#include "vnode_multi_function_wrapper_builder.h"

namespace FN {

using BLI::float3;

static void BUILD_vector_socket(VSocketMFBuilder &builder)
{
  BLI::float3 value;
  RNA_float_get_array(builder.vsocket().rna(), "value", value);
  builder.build_constant_value_fn(value);
}

static void BUILD_color_socket(VSocketMFBuilder &builder)
{
  BLI::rgba_f value;
  RNA_float_get_array(builder.vsocket().rna(), "value", value);
  builder.build_constant_value_fn(value);
}

static void BUILD_float_socket(VSocketMFBuilder &builder)
{
  float value = RNA_float_get(builder.vsocket().rna(), "value");
  builder.build_constant_value_fn(value);
}

static void BUILD_bool_socket(VSocketMFBuilder &builder)
{
  bool value = RNA_boolean_get(builder.vsocket().rna(), "value");
  builder.build_constant_value_fn(value);
}

static void BUILD_int_socket(VSocketMFBuilder &builder)
{
  int value = RNA_int_get(builder.vsocket().rna(), "value");
  builder.build_constant_value_fn(value);
}

static void BUILD_object_socket(VSocketMFBuilder &builder)
{
  Object *value = (Object *)RNA_pointer_get(builder.vsocket().rna(), "value").data;
  builder.build_constant_value_fn(value);
}

static void BUILD_text_socket(VSocketMFBuilder &builder)
{
  char *value = RNA_string_get_alloc(builder.vsocket().rna(), "value", nullptr, 0);
  std::string text = value;
  MEM_freeN(value);

  builder.build_constant_value_fn(text);
}

template<typename T> static void BUILD_empty_list_socket(VSocketMFBuilder &builder)
{
  const MultiFunction &fn = builder.construct_fn<MF_EmptyList<T>>();
  builder.set_fn(fn);
}

static void WRAP_combine_color(VNodeMFWrapperBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_CombineColor>(
      {"use_list__red", "use_list__green", "use_list__blue", "use_list__alpha"});
}

static void WRAP_separate_color(VNodeMFWrapperBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_SeparateColor>({"use_list__color"});
}

static void WRAP_combine_vector(VNodeMFWrapperBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_CombineVector>(
      {"use_list__x", "use_list__y", "use_list__z"});
}

static void WRAP_separate_vector(VNodeMFWrapperBuilder &builder)
{
  builder.set_vectorized_constructed_matching_fn<MF_SeparateVector>({"use_list__vector"});
}

static void WRAP_list_length(VNodeMFWrapperBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  builder.set_constructed_matching_fn<MF_ListLength>(type);
}

static void WRAP_get_list_element(VNodeMFWrapperBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  builder.set_constructed_matching_fn<MF_GetListElement>(type);
}

static void WRAP_pack_list(VNodeMFWrapperBuilder &builder)
{
  const CPPType &type = builder.cpp_type_from_property("active_type");
  Vector<bool> list_states = builder.get_list_base_variadic_states("variadic");
  builder.set_constructed_matching_fn<MF_PackList>(type, list_states);
}

static void WRAP_object_location(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ObjectWorldLocation>();
}

static void WRAP_object_mesh_info(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ObjectVertexPositions>();
}

static void WRAP_switch(VNodeMFWrapperBuilder &builder)
{
  MFDataType type = builder.data_type_from_property("data_type");
  switch (type.category()) {
    case MFDataType::Single: {
      builder.set_constructed_matching_fn<MF_SwitchSingle>(type.type());
      break;
    }
    case MFDataType::Vector: {
      builder.set_constructed_matching_fn<MF_SwitchVector>(type.type());
      break;
    }
  }
}

static void WRAP_text_length(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_TextLength>();
}

static void WRAP_vertex_info(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ContextVertexPosition>();
}

static void WRAP_float_range(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_FloatRange>();
}

static void WRAP_time_info(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ContextCurrentFrame>();
}

template<typename InT, typename OutT>
static void build_math_fn(VNodeMFWrapperBuilder &builder, OutT (*func)(InT))
{
  auto fn =
      [func](MFMask mask, VirtualListRef<InT> inputs, MutableArrayRef<OutT> outputs) -> void {
    for (uint i : mask.indices()) {
      new (&outputs[i]) OutT(func(inputs[i]));
    }
  };

  builder.set_vectorized_constructed_matching_fn<MF_Custom_In1_Out1<InT, OutT>>(
      {"use_list"}, builder.vnode().name(), fn);
}

template<typename InT1, typename InT2, typename OutT>
static void build_math_fn(VNodeMFWrapperBuilder &builder, OutT (*func)(InT1, InT2))
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
      {"use_list__a", "use_list__b"}, builder.vnode().name(), fn);
}

template<typename T>
static void build_variadic_math_fn(VNodeMFWrapperBuilder &builder,
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
        builder.vnode().name(), list_states.size(), fn);
    if (list_states.contains(true)) {
      builder.set_constructed_matching_fn<MF_SimpleVectorize>(base_fn, list_states);
    }
    else {
      builder.set_matching_fn(base_fn);
    }
  }
}

static void WRAP_add_floats(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](float a, float b) -> float { return a + b; }, 0.0f);
}

static void WRAP_multiply_floats(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](float a, float b) -> float { return a * b; }, 1.0f);
}

static void WRAP_minimum_floats(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](float a, float b) -> float { return std::min(a, b); }, 0.0f);
}

static void WRAP_maximum_floats(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](float a, float b) -> float { return std::max(a, b); }, 0.0f);
}

static void WRAP_subtract_floats(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a, float b) -> float { return a - b; });
}

static void WRAP_divide_floats(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a, float b) -> float { return (b != 0.0f) ? a / b : 0.0f; });
}

static void WRAP_power_floats(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder,
      +[](float a, float b) -> float { return (a >= 0.0f) ? (float)std::pow(a, b) : 0.0f; });
}

static void WRAP_sqrt_float(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a) -> float { return (a >= 0.0f) ? (float)std::sqrt(a) : 0.0f; });
}

static void WRAP_abs_float(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a) -> float { return std::abs(a); });
}

static void WRAP_sine_float(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a) -> float { return std::sin(a); });
}

static void WRAP_cosine_float(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a) -> float { return std::cos(a); });
}

static void WRAP_add_vectors(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float3 a, float3 b) -> float3 { return a + b; }, {0, 0, 0});
}

static void WRAP_multiply_vectors(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(builder, +[](float3 a, float3 b) -> float3 { return a * b; }, {1, 1, 1});
}

static void WRAP_subtract_vectors(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float3 a, float3 b) -> float3 { return a - b; });
}

static void WRAP_divide_vectors(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(builder, float3::safe_divide);
}

static void WRAP_vector_cross_product(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(builder, float3::cross_high_precision);
}

static void WRAP_reflect_vector(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float3 a, float3 b) { return a.reflected(b.normalized()); });
}

static void WRAP_project_vector(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(builder, float3::project);
}

static void WRAP_vector_dot_product(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(builder, float3::dot);
}

static void WRAP_vector_distance(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(builder, float3::distance);
}

static void WRAP_boolean_and(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](bool a, bool b) { return a && b; }, true);
}

static void WRAP_boolean_or(VNodeMFWrapperBuilder &builder)
{
  build_variadic_math_fn(
      builder, +[](bool a, bool b) { return a || b; }, false);
}

static void WRAP_boolean_not(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](bool a) -> bool { return !a; });
}

static void WRAP_compare(VNodeMFWrapperBuilder &builder)
{
  build_math_fn(
      builder, +[](float a, float b) -> bool { return a < b; });
}

static void WRAP_perlin_noise(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_PerlinNoise>();
}

static void WRAP_particle_info(VNodeMFWrapperBuilder &builder)
{
  const VNode &vnode = builder.vnode();

  Vector<std::string> attribute_names;
  Vector<const CPPType *> attribute_types;
  Vector<VSocketsForMFParam> param_vsockets;

  if (builder.output_is_required(vnode.output(0, "ID"))) {
    attribute_names.append("ID");
    attribute_types.append(&CPP_TYPE<int>());
    param_vsockets.append({nullptr, &vnode.output(0, "ID")});
  }
  if (builder.output_is_required(vnode.output(1, "Position"))) {
    attribute_names.append("Position");
    attribute_types.append(&CPP_TYPE<float3>());
    param_vsockets.append({nullptr, &vnode.output(1, "Position")});
  }
  if (builder.output_is_required(vnode.output(2, "Velocity"))) {
    attribute_names.append("Velocity");
    attribute_types.append(&CPP_TYPE<float3>());
    param_vsockets.append({nullptr, &vnode.output(2, "Velocity")});
  }
  if (builder.output_is_required(vnode.output(3, "Birth Time"))) {
    attribute_names.append("Birth Time");
    attribute_types.append(&CPP_TYPE<float>());
    param_vsockets.append({nullptr, &vnode.output(3, "Birth Time")});
  }

  const MultiFunction &fn = builder.construct_fn<MF_ParticleAttributes>(attribute_names,
                                                                        attribute_types);
  builder.set_fn(fn, param_vsockets);
}

static void WRAP_closest_point_on_object(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_ClosestPointOnObject>();
}

static void WRAP_clamp_float(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_Clamp>(false);
}

static void WRAP_map_range(VNodeMFWrapperBuilder &builder)
{
  bool clamp = RNA_boolean_get(builder.vnode().rna(), "clamp");
  builder.set_constructed_matching_fn<MF_MapRange>(clamp);
}

// static void WRAP_group(VNodeMFWrapperBuilder &builder)

static void WRAP_random_float(VNodeMFWrapperBuilder &builder)
{
  builder.set_constructed_matching_fn<MF_RandomFloat>();
}

template<typename T>
static void add_basic_type(VTreeMFMappings &mappings,
                           StringRef base_name,
                           BuildVSocketMFFunc vsocket_fn_builder)
{
  std::string base_idname = "fn_" + base_name + "Socket";
  std::string list_idname = "fn_" + base_name + "ListSocket";
  std::string list_name = base_name + " List";

  mappings.cpp_type_by_name.add_new(base_name, &CPP_TYPE<T>());
  mappings.name_from_cpp_type.add_new(&CPP_TYPE<T>(), base_name);

  mappings.data_type_by_idname.add_new(base_idname, MFDataType::ForSingle<T>());
  mappings.data_type_by_idname.add_new(list_idname, MFDataType::ForVector<T>());

  mappings.data_type_by_name.add_new(base_name, MFDataType::ForSingle<T>());
  mappings.data_type_by_name.add_new(list_name, MFDataType::ForVector<T>());

  mappings.vsocket_builders.add_new(base_idname, vsocket_fn_builder);
  mappings.vsocket_builders.add_new(list_idname, BUILD_empty_list_socket<T>);

  mappings.conversion_functions.add_new({base_idname, list_idname},
                                        BLI::make_unique<MF_SingleElementList<T>>());
}

template<typename FromT, typename ToT>
static void add_implicit_conversion(VTreeMFMappings &mappings)
{
  StringRef from_name = mappings.name_from_cpp_type.lookup(&CPP_TYPE<FromT>());
  StringRef to_name = mappings.name_from_cpp_type.lookup(&CPP_TYPE<ToT>());

  std::string from_base_idname = "fn_" + from_name + "Socket";
  std::string from_list_idname = "fn_" + from_name + "ListSocket";

  std::string to_base_idname = "fn_" + to_name + "Socket";
  std::string to_list_idname = "fn_" + to_name + "ListSocket";

  mappings.conversion_functions.add_new({from_base_idname, to_base_idname},
                                        BLI::make_unique<MF_Convert<FromT, ToT>>());
  mappings.conversion_functions.add_new({from_list_idname, to_list_idname},
                                        BLI::make_unique<MF_ConvertList<FromT, ToT>>());
}

template<typename T1, typename T2>
static void add_bidirectional_implicit_conversion(VTreeMFMappings &mappings)
{
  add_implicit_conversion<T1, T2>(mappings);
  add_implicit_conversion<T2, T1>(mappings);
}

static void add_vnode_wrapper(VTreeMFMappings &mappings,
                              StringRef idname,
                              BuildVNodeMFWrapperFunc func)
{
  mappings.vnode_builders.add_new(idname, func);
}

BLI_LAZY_INIT_REF(const VTreeMFMappings, get_vtree_mf_mappings)
{
  auto mappings_ = BLI::make_unique<VTreeMFMappings>();
  VTreeMFMappings &mappings = *mappings_;

  add_basic_type<float>(mappings, "Float", BUILD_float_socket);
  add_basic_type<BLI::float3>(mappings, "Vector", BUILD_vector_socket);
  add_basic_type<int32_t>(mappings, "Integer", BUILD_int_socket);
  add_basic_type<Object *>(mappings, "Object", BUILD_object_socket);
  add_basic_type<std::string>(mappings, "Text", BUILD_text_socket);
  add_basic_type<bool>(mappings, "Boolean", BUILD_bool_socket);
  add_basic_type<BLI::rgba_f>(mappings, "Color", BUILD_color_socket);

  add_bidirectional_implicit_conversion<float, int32_t>(mappings);
  add_bidirectional_implicit_conversion<float, bool>(mappings);
  add_bidirectional_implicit_conversion<int32_t, bool>(mappings);

  add_vnode_wrapper(mappings, "fn_CombineColorNode", WRAP_combine_color);
  add_vnode_wrapper(mappings, "fn_SeparateColorNode", WRAP_separate_color);
  add_vnode_wrapper(mappings, "fn_CombineVectorNode", WRAP_combine_vector);
  add_vnode_wrapper(mappings, "fn_SeparateVectorNode", WRAP_separate_vector);
  add_vnode_wrapper(mappings, "fn_SwitchNode", WRAP_switch);
  add_vnode_wrapper(mappings, "fn_ListLengthNode", WRAP_list_length);
  add_vnode_wrapper(mappings, "fn_PackListNode", WRAP_pack_list);
  add_vnode_wrapper(mappings, "fn_GetListElementNode", WRAP_get_list_element);
  add_vnode_wrapper(mappings, "fn_ObjectTransformsNode", WRAP_object_location);
  add_vnode_wrapper(mappings, "fn_ObjectMeshNode", WRAP_object_mesh_info);
  add_vnode_wrapper(mappings, "fn_TextLengthNode", WRAP_text_length);
  add_vnode_wrapper(mappings, "fn_VertexInfoNode", WRAP_vertex_info);
  add_vnode_wrapper(mappings, "fn_FloatRangeNode", WRAP_float_range);
  add_vnode_wrapper(mappings, "fn_TimeInfoNode", WRAP_time_info);
  add_vnode_wrapper(mappings, "fn_CompareNode", WRAP_compare);
  add_vnode_wrapper(mappings, "fn_PerlinNoiseNode", WRAP_perlin_noise);
  add_vnode_wrapper(mappings, "fn_ParticleInfoNode", WRAP_particle_info);
  add_vnode_wrapper(mappings, "fn_ClosestPointOnObjectNode", WRAP_closest_point_on_object);
  add_vnode_wrapper(mappings, "fn_MapRangeNode", WRAP_map_range);
  add_vnode_wrapper(mappings, "fn_FloatClampNode", WRAP_clamp_float);
  // add_vnode_wrapper(mappings, "fn_GroupNode", WRAP_group_node);
  add_vnode_wrapper(mappings, "fn_RandomFloatNode", WRAP_random_float);

  add_vnode_wrapper(mappings, "fn_AddFloatsNode", WRAP_add_floats);
  add_vnode_wrapper(mappings, "fn_MultiplyFloatsNode", WRAP_multiply_floats);
  add_vnode_wrapper(mappings, "fn_MinimumFloatsNode", WRAP_minimum_floats);
  add_vnode_wrapper(mappings, "fn_MaximumFloatsNode", WRAP_maximum_floats);

  add_vnode_wrapper(mappings, "fn_SubtractFloatsNode", WRAP_subtract_floats);
  add_vnode_wrapper(mappings, "fn_DivideFloatsNode", WRAP_divide_floats);
  add_vnode_wrapper(mappings, "fn_PowerFloatsNode", WRAP_power_floats);

  add_vnode_wrapper(mappings, "fn_SqrtFloatNode", WRAP_sqrt_float);
  add_vnode_wrapper(mappings, "fn_AbsoluteFloatNode", WRAP_abs_float);
  add_vnode_wrapper(mappings, "fn_SineFloatNode", WRAP_sine_float);
  add_vnode_wrapper(mappings, "fn_CosineFloatNode", WRAP_cosine_float);

  add_vnode_wrapper(mappings, "fn_AddVectorsNode", WRAP_add_vectors);
  add_vnode_wrapper(mappings, "fn_SubtractVectorsNode", WRAP_subtract_vectors);
  add_vnode_wrapper(mappings, "fn_MultiplyVectorsNode", WRAP_multiply_vectors);
  add_vnode_wrapper(mappings, "fn_DivideVectorsNode", WRAP_divide_vectors);

  add_vnode_wrapper(mappings, "fn_VectorCrossProductNode", WRAP_vector_cross_product);
  add_vnode_wrapper(mappings, "fn_ReflectVectorNode", WRAP_reflect_vector);
  add_vnode_wrapper(mappings, "fn_ProjectVectorNode", WRAP_project_vector);
  add_vnode_wrapper(mappings, "fn_VectorDotProductNode", WRAP_vector_dot_product);
  add_vnode_wrapper(mappings, "fn_VectorDistanceNode", WRAP_vector_distance);

  add_vnode_wrapper(mappings, "fn_BooleanAndNode", WRAP_boolean_and);
  add_vnode_wrapper(mappings, "fn_BooleanOrNode", WRAP_boolean_or);
  add_vnode_wrapper(mappings, "fn_BooleanNotNode", WRAP_boolean_not);

  return mappings_;
}

}  // namespace FN
