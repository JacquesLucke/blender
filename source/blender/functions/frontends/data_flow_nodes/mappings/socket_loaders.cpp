#include "FN_types.hpp"
#include "FN_functions.hpp"

#include "RNA_access.h"

#include "registry.hpp"

namespace FN {
namespace DataFlowNodes {

using BLI::float3;
using BLI::rgba_f;
using namespace FN::Types;

static void LOAD_float(PointerRNA *rna, Tuple &tuple, uint index)
{
  float value = RNA_float_get(rna, "value");
  tuple.set<float>(index, value);
}

static void LOAD_vector(PointerRNA *rna, Tuple &tuple, uint index)
{
  float3 vector;
  RNA_float_get_array(rna, "value", vector);
  tuple.set<float3>(index, vector);
}

static void LOAD_integer(PointerRNA *rna, Tuple &tuple, uint index)
{
  int value = RNA_int_get(rna, "value");
  tuple.set<int32_t>(index, value);
}

static void LOAD_boolean(PointerRNA *rna, Tuple &tuple, uint index)
{
  bool value = RNA_boolean_get(rna, "value");
  tuple.set<bool>(index, value);
}

static void LOAD_object(PointerRNA *rna, Tuple &tuple, uint index)
{
  Object *value = (Object *)RNA_pointer_get(rna, "value").id.data;
  tuple.set<Object *>(index, value);
}

static void LOAD_color(PointerRNA *rna, Tuple &tuple, uint index)
{
  rgba_f color;
  RNA_float_get_array(rna, "value", color);
  tuple.set<rgba_f>(index, color);
}

static SocketLoader GET_empty_list_loader(SharedType &type)
{
  return [&type](PointerRNA *UNUSED(rna), Tuple &tuple, uint index) {
    auto list = SharedList::New(type);
    tuple.move_in(index, list);
  };
}

void REGISTER_socket_loaders(std::unique_ptr<SocketLoaders> &loaders)
{
  loaders->register_loader("Boolean List", GET_empty_list_loader(GET_TYPE_bool()));
  loaders->register_loader("Boolean", LOAD_boolean);
  loaders->register_loader("Color List", GET_empty_list_loader(GET_TYPE_rgba_f()));
  loaders->register_loader("Color", LOAD_color);
  loaders->register_loader("Float List", GET_empty_list_loader(GET_TYPE_float()));
  loaders->register_loader("Float", LOAD_float);
  loaders->register_loader("Integer List", GET_empty_list_loader(GET_TYPE_int32()));
  loaders->register_loader("Integer", LOAD_integer);
  loaders->register_loader("Object List", GET_empty_list_loader(GET_TYPE_object()));
  loaders->register_loader("Object", LOAD_object);
  loaders->register_loader("Vector List", GET_empty_list_loader(GET_TYPE_float3()));
  loaders->register_loader("Vector", LOAD_vector);
}

}  // namespace DataFlowNodes
}  // namespace FN
