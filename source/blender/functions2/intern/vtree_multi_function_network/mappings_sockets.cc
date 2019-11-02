#include "mappings.h"
#include "builder.h"

#include "BLI_math_cxx.h"

#include "FN_multi_functions.h"

namespace FN {

template<typename T, typename... Args>
T &allocate_resource(const char *name, OwnedResources &resources, Args &&... args)
{
  std::unique_ptr<T> value = BLI::make_unique<T>(std::forward<Args>(args)...);
  T &value_ref = *value;
  resources.add(std::move(value), name);
  return value_ref;
}

/* Socket Inserters
 **********************************************************/

static MFBuilderOutputSocket &INSERT_vector_socket(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VSocket &vsocket)
{
  BLI::float3 value;
  RNA_float_get_array(vsocket.rna(), "value", value);

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<BLI::float3>>(
      "vector socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_float_socket(VTreeMFNetworkBuilder &builder,
                                                  OwnedResources &resources,
                                                  const VSocket &vsocket)
{
  float value = RNA_float_get(vsocket.rna(), "value");

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<float>>(
      "float socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_bool_socket(VTreeMFNetworkBuilder &builder,
                                                 OwnedResources &resources,
                                                 const VSocket &vsocket)
{
  bool value = RNA_boolean_get(vsocket.rna(), "value");

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<bool>>(
      "boolean socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_int_socket(VTreeMFNetworkBuilder &builder,
                                                OwnedResources &resources,
                                                const VSocket &vsocket)
{
  int value = RNA_int_get(vsocket.rna(), "value");

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<int>>(
      "int socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_object_socket(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VSocket &vsocket)
{
  Object *value = (Object *)RNA_pointer_get(vsocket.rna(), "value").data;

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<Object *>>(
      "object socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_text_socket(VTreeMFNetworkBuilder &builder,
                                                 OwnedResources &resources,
                                                 const VSocket &vsocket)
{
  char *value = RNA_string_get_alloc(vsocket.rna(), "value", nullptr, 0);
  std::string text = value;
  MEM_freeN(value);

  const MultiFunction &fn = allocate_resource<FN::MF_ConstantValue<std::string>>(
      "text socket", resources, text);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

template<typename T>
static MFBuilderOutputSocket &INSERT_empty_list_socket(VTreeMFNetworkBuilder &builder,
                                                       OwnedResources &resources,
                                                       const VSocket &UNUSED(vsocket))
{
  const MultiFunction &fn = allocate_resource<FN::MF_EmptyList<T>>("empty list socket", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

/* Implicit Conversion Inserters
 *******************************************/

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<FN::MF_Convert<FromT, ToT>>("converter function",
                                                                          resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert_list(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<FN::MF_ConvertList<FromT, ToT>>(
      "convert list function", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename T>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_element_to_list(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<FN::MF_SingleElementList<T>>(
      "single element list function", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename T>
static void add_basic_type(VTreeMultiFunctionMappings &mappings,
                           StringRef base_name,
                           InsertUnlinkedInputFunction base_inserter)
{
  std::string base_idname = "fn_" + base_name + "Socket";
  std::string list_idname = "fn_" + base_name + "ListSocket";
  std::string list_name = base_name + " List";

  mappings.cpp_type_by_type_name.add_new(base_name, &GET_TYPE<T>());
  mappings.data_type_by_idname.add_new(base_idname, MFDataType::ForSingle<T>());
  mappings.data_type_by_idname.add_new(list_idname, MFDataType::ForVector<T>());
  mappings.input_inserters.add_new(base_idname, base_inserter);
  mappings.input_inserters.add_new(list_idname, INSERT_empty_list_socket<T>);
  mappings.conversion_inserters.add_new({base_idname, list_idname}, INSERT_element_to_list<T>);
  mappings.type_name_from_cpp_type.add_new(&GET_TYPE<T>(), base_name);
}

template<typename FromT, typename ToT>
static void add_implicit_conversion(VTreeMultiFunctionMappings &mappings)
{
  StringRef from_name = mappings.type_name_from_cpp_type.lookup(&GET_TYPE<FromT>());
  StringRef to_name = mappings.type_name_from_cpp_type.lookup(&GET_TYPE<ToT>());

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

void add_vtree_socket_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  add_basic_type<float>(mappings, "Float", INSERT_float_socket);
  add_basic_type<BLI::float3>(mappings, "Vector", INSERT_vector_socket);
  add_basic_type<int32_t>(mappings, "Integer", INSERT_int_socket);
  add_basic_type<Object *>(mappings, "Object", INSERT_object_socket);
  add_basic_type<std::string>(mappings, "Text", INSERT_text_socket);
  add_basic_type<bool>(mappings, "Boolean", INSERT_bool_socket);

  add_bidirectional_implicit_conversion<float, int32_t>(mappings);
  add_bidirectional_implicit_conversion<float, bool>(mappings);
  add_bidirectional_implicit_conversion<int32_t, bool>(mappings);
}

};  // namespace FN
