#include "builder.h"
#include "mappings.h"

#include "BLI_color.h"
#include "BLI_float3.h"

#include "BKE_surface_hook.h"

#include "FN_multi_functions.h"

namespace FN {
namespace MFGeneration {

/* Socket Inserters
 **********************************************************/

static void INSERT_vector_socket(VSocketMFBuilder &builder)
{
  BLI::float3 value;
  RNA_float_get_array(builder.rna(), "value", value);
  builder.set_constant_value(value);
}

static void INSERT_color_socket(VSocketMFBuilder &builder)
{
  BLI::rgba_f value;
  RNA_float_get_array(builder.rna(), "value", value);
  builder.set_constant_value(value);
}

static void INSERT_float_socket(VSocketMFBuilder &builder)
{
  float value = RNA_float_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_bool_socket(VSocketMFBuilder &builder)
{
  bool value = RNA_boolean_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_int_socket(VSocketMFBuilder &builder)
{
  int value = RNA_int_get(builder.rna(), "value");
  builder.set_constant_value(value);
}

static void INSERT_object_socket(VSocketMFBuilder &builder)
{
  Object *value = (Object *)RNA_pointer_get(builder.rna(), "value").data;
  if (value == nullptr) {
    builder.set_constant_value(BKE::ObjectIDHandle());
  }
  else {
    builder.set_constant_value(BKE::ObjectIDHandle(value));
  }
}

static void INSERT_image_socket(VSocketMFBuilder &builder)
{
  Image *value = (Image *)RNA_pointer_get(builder.rna(), "value").data;
  if (value == nullptr) {
    builder.set_constant_value(BKE::ImageIDHandle());
  }
  else {
    builder.set_constant_value(BKE::ImageIDHandle(value));
  }
}

static void INSERT_text_socket(VSocketMFBuilder &builder)
{
  char *value = RNA_string_get_alloc(builder.rna(), "value", nullptr, 0);
  std::string text = value;
  MEM_freeN(value);
  builder.set_constant_value(std::move(text));
}

static void INSERT_surface_hook_socket(VSocketMFBuilder &builder)
{
  builder.set_constant_value(BKE::SurfaceHook());
}

template<typename T> static void INSERT_empty_list_socket(VSocketMFBuilder &builder)
{
  const MultiFunction &fn = builder.construct_fn<FN::MF_EmptyList<T>>();
  builder.set_generator_fn(fn);
}

/* Implicit Conversion Inserters
 *******************************************/

template<typename FromT, typename ToT> static void INSERT_convert(ConversionMFBuilder &builder)
{
  builder.set_constructed_conversion_fn<MF_Convert<FromT, ToT>>();
}

template<typename FromT, typename ToT>
static void INSERT_convert_list(ConversionMFBuilder &builder)
{
  builder.set_constructed_conversion_fn<MF_ConvertList<FromT, ToT>>();
}

template<typename T> static void INSERT_element_to_list(ConversionMFBuilder &builder)
{
  builder.set_constructed_conversion_fn<MF_SingleElementList<T>>();
}

template<typename T>
static void add_basic_type(FunctionTreeMFMappings &mappings,
                           StringRef base_name,
                           StringRef base_name_without_spaces,
                           VSocketInserter base_inserter)
{
  std::string base_idname = "fn_" + base_name_without_spaces + "Socket";
  std::string list_idname = "fn_" + base_name_without_spaces + "ListSocket";
  std::string list_name = base_name + " List";

  const CPPType &cpp_type = CPP_TYPE<T>();
  MFDataType base_data_type = MFDataType::ForSingle(cpp_type);
  MFDataType list_data_type = MFDataType::ForVector(cpp_type);

  mappings.cpp_type_by_type_name.add_new(base_name, &cpp_type);
  mappings.data_type_by_idname.add_new(base_idname, base_data_type);
  mappings.data_type_by_idname.add_new(list_idname, list_data_type);
  mappings.data_type_by_type_name.add_new(base_name, base_data_type);
  mappings.data_type_by_type_name.add_new(list_name, list_data_type);
  mappings.fsocket_inserters.add_new(base_idname, base_inserter);
  mappings.fsocket_inserters.add_new(list_idname, INSERT_empty_list_socket<T>);
  mappings.conversion_inserters.add_new({base_data_type, list_data_type},
                                        INSERT_element_to_list<T>);
  mappings.type_name_from_cpp_type.add_new(&cpp_type, base_name);
}

template<typename T>
static void add_basic_type(FunctionTreeMFMappings &mappings,
                           StringRef base_name,
                           VSocketInserter base_inserter)
{
  add_basic_type<T>(mappings, base_name, base_name, base_inserter);
}

template<typename FromT, typename ToT>
static void add_implicit_conversion(FunctionTreeMFMappings &mappings)
{
  StringRef from_name = mappings.type_name_from_cpp_type.lookup(&CPP_TYPE<FromT>());
  StringRef to_name = mappings.type_name_from_cpp_type.lookup(&CPP_TYPE<ToT>());

  std::string from_base_idname = "fn_" + from_name + "Socket";
  std::string from_list_idname = "fn_" + from_name + "ListSocket";

  std::string to_base_idname = "fn_" + to_name + "Socket";
  std::string to_list_idname = "fn_" + to_name + "ListSocket";

  mappings.conversion_inserters.add_new(
      {MFDataType::ForSingle<FromT>(), MFDataType::ForSingle<ToT>()}, INSERT_convert<FromT, ToT>);
  mappings.conversion_inserters.add_new(
      {MFDataType::ForVector<FromT>(), MFDataType::ForVector<ToT>()},
      INSERT_convert_list<FromT, ToT>);
}

template<typename T1, typename T2>
static void add_bidirectional_implicit_conversion(FunctionTreeMFMappings &mappings)
{
  add_implicit_conversion<T1, T2>(mappings);
  add_implicit_conversion<T2, T1>(mappings);
}

void add_function_tree_socket_mapping_info(FunctionTreeMFMappings &mappings)
{
  add_basic_type<float>(mappings, "Float", INSERT_float_socket);
  add_basic_type<BLI::float3>(mappings, "Vector", INSERT_vector_socket);
  add_basic_type<int32_t>(mappings, "Integer", INSERT_int_socket);
  add_basic_type<BKE::ObjectIDHandle>(mappings, "Object", INSERT_object_socket);
  add_basic_type<BKE::ImageIDHandle>(mappings, "Image", INSERT_image_socket);
  add_basic_type<std::string>(mappings, "Text", INSERT_text_socket);
  add_basic_type<bool>(mappings, "Boolean", INSERT_bool_socket);
  add_basic_type<BLI::rgba_f>(mappings, "Color", INSERT_color_socket);
  add_basic_type<BKE::SurfaceHook>(
      mappings, "Surface Hook", "SurfaceHook", INSERT_surface_hook_socket);

  add_bidirectional_implicit_conversion<float, int32_t>(mappings);
  add_bidirectional_implicit_conversion<float, bool>(mappings);
  add_bidirectional_implicit_conversion<int32_t, bool>(mappings);
}

}  // namespace MFGeneration
}  // namespace FN
