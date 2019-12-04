#include "mappings.h"
#include "builder.h"

#include "BLI_math_cxx.h"

#include "BKE_surface_location.h"

#include "FN_multi_functions.h"

namespace FN {

/* Socket Inserters
 **********************************************************/

static void INSERT_vector_socket(VSocketMFNetworkBuilder &builder)
{
  BLI::float3 value;
  RNA_float_get_array(builder.rna(), "value", value);
  builder.set_constant_value(value);
}

static void INSERT_color_socket(VSocketMFNetworkBuilder &builder)
{
  BLI::rgba_f value;
  RNA_float_get_array(builder.rna(), "value", value);
  builder.set_constant_value(value);
}

static void INSERT_float_socket(VSocketMFNetworkBuilder &builder)
{
  float value = RNA_float_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_bool_socket(VSocketMFNetworkBuilder &builder)
{
  bool value = RNA_boolean_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_int_socket(VSocketMFNetworkBuilder &builder)
{
  int value = RNA_int_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_object_socket(VSocketMFNetworkBuilder &builder)
{
  Object *value = (Object *)RNA_pointer_get(builder.rna(), "value").data;
  if (value == nullptr) {
    builder.set_constant_value(BKE::ObjectIDHandle());
  }
  else {
    builder.set_constant_value(BKE::ObjectIDHandle(value));
  }
}

static void INSERT_text_socket(VSocketMFNetworkBuilder &builder)
{
  char *value = RNA_string_get_alloc(builder.rna(), "value", nullptr, 0);
  std::string text = value;
  MEM_freeN(value);
  builder.set_constant_value(std::move(text));
}

static void INSERT_surface_location_socket(VSocketMFNetworkBuilder &builder)
{
  builder.set_constant_value(BKE::SurfaceLocation());
}

template<typename T> static void INSERT_empty_list_socket(VSocketMFNetworkBuilder &builder)
{
  const MultiFunction &fn = builder.network_builder().construct_fn<FN::MF_EmptyList<T>>();
  builder.set_generator_fn(fn);
}

/* Implicit Conversion Inserters
 *******************************************/

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert(
    VTreeMFNetworkBuilder &builder)
{
  const MultiFunction &fn = builder.construct_fn<FN::MF_Convert<FromT, ToT>>();
  MFBuilderFunctionNode &node = builder.add_function(fn);
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert_list(
    VTreeMFNetworkBuilder &builder)
{
  const MultiFunction &fn = builder.construct_fn<FN::MF_ConvertList<FromT, ToT>>();
  MFBuilderFunctionNode &node = builder.add_function(fn);
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename T>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_element_to_list(
    VTreeMFNetworkBuilder &builder)
{
  const MultiFunction &fn = builder.construct_fn<FN::MF_SingleElementList<T>>();
  MFBuilderFunctionNode &node = builder.add_function(fn);
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename T>
static void add_basic_type(VTreeMultiFunctionMappings &mappings,
                           StringRef base_name,
                           StringRef base_name_without_spaces,
                           InsertVSocketFunction base_inserter)
{
  std::string base_idname = "fn_" + base_name_without_spaces + "Socket";
  std::string list_idname = "fn_" + base_name_without_spaces + "ListSocket";
  std::string list_name = base_name + " List";

  mappings.cpp_type_by_type_name.add_new(base_name, &CPP_TYPE<T>());
  mappings.data_type_by_idname.add_new(base_idname, MFDataType::ForSingle<T>());
  mappings.data_type_by_idname.add_new(list_idname, MFDataType::ForVector<T>());
  mappings.data_type_by_type_name.add_new(base_name, MFDataType::ForSingle<T>());
  mappings.data_type_by_type_name.add_new(list_name, MFDataType::ForVector<T>());
  mappings.xsocket_inserters.add_new(base_idname, base_inserter);
  mappings.xsocket_inserters.add_new(list_idname, INSERT_empty_list_socket<T>);
  mappings.conversion_inserters.add_new({base_idname, list_idname}, INSERT_element_to_list<T>);
  mappings.type_name_from_cpp_type.add_new(&CPP_TYPE<T>(), base_name);
}

template<typename T>
static void add_basic_type(VTreeMultiFunctionMappings &mappings,
                           StringRef base_name,
                           InsertVSocketFunction base_inserter)
{
  add_basic_type<T>(mappings, base_name, base_name, base_inserter);
}

template<typename FromT, typename ToT>
static void add_implicit_conversion(VTreeMultiFunctionMappings &mappings)
{
  StringRef from_name = mappings.type_name_from_cpp_type.lookup(&CPP_TYPE<FromT>());
  StringRef to_name = mappings.type_name_from_cpp_type.lookup(&CPP_TYPE<ToT>());

  std::string from_base_idname = "fn_" + from_name + "Socket";
  std::string from_list_idname = "fn_" + from_name + "ListSocket";

  std::string to_base_idname = "fn_" + to_name + "Socket";
  std::string to_list_idname = "fn_" + to_name + "ListSocket";

  mappings.conversion_inserters.add_new({from_base_idname, to_base_idname},
                                        INSERT_convert<FromT, ToT>);
  mappings.conversion_inserters.add_new({from_list_idname, to_list_idname},
                                        INSERT_convert_list<FromT, ToT>);
}

template<typename T1, typename T2>
static void add_bidirectional_implicit_conversion(VTreeMultiFunctionMappings &mappings)
{
  add_implicit_conversion<T1, T2>(mappings);
  add_implicit_conversion<T2, T1>(mappings);
}

void add_inlined_tree_socket_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  add_basic_type<float>(mappings, "Float", INSERT_float_socket);
  add_basic_type<BLI::float3>(mappings, "Vector", INSERT_vector_socket);
  add_basic_type<int32_t>(mappings, "Integer", INSERT_int_socket);
  add_basic_type<BKE::ObjectIDHandle>(mappings, "Object", INSERT_object_socket);
  add_basic_type<std::string>(mappings, "Text", INSERT_text_socket);
  add_basic_type<bool>(mappings, "Boolean", INSERT_bool_socket);
  add_basic_type<BLI::rgba_f>(mappings, "Color", INSERT_color_socket);
  add_basic_type<BKE::SurfaceLocation>(
      mappings, "Surface Location", "SurfaceLocation", INSERT_surface_location_socket);

  add_bidirectional_implicit_conversion<float, int32_t>(mappings);
  add_bidirectional_implicit_conversion<float, bool>(mappings);
  add_bidirectional_implicit_conversion<int32_t, bool>(mappings);
}

};  // namespace FN
