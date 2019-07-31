#include "../registry.hpp"

#include "FN_types.hpp"
#include "FN_functions.hpp"

#include "RNA_access.h"

namespace FN {
namespace DataFlowNodes {

using BLI::float3;
using BLI::rgba_f;

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

template<typename T> static void LOAD_empty_list(PointerRNA *UNUSED(rna), Tuple &tuple, uint index)
{
  auto list = Types::SharedList<T>::New();
  tuple.move_in(index, list);
}

void register_socket_loaders(SocketLoaderRegistry &registry)
{
  registry.loader("fn_FloatSocket", LOAD_float);
  registry.loader("fn_VectorSocket", LOAD_vector);
  registry.loader("fn_IntegerSocket", LOAD_integer);
  registry.loader("fn_BooleanSocket", LOAD_boolean);
  registry.loader("fn_ObjectSocket", LOAD_object);
  registry.loader("fn_ColorSocket", LOAD_color);
  registry.loader("fn_FloatListSocket", LOAD_empty_list<float>);
  registry.loader("fn_VectorListSocket", LOAD_empty_list<float3>);
  registry.loader("fn_IntegerListSocket", LOAD_empty_list<int32_t>);
  registry.loader("fn_BooleanListSocket", LOAD_empty_list<bool>);
  registry.loader("fn_ObjectListSocket", LOAD_empty_list<Object *>);
  registry.loader("fn_ColorListSocket", LOAD_empty_list<rgba_f>);
}

}  // namespace DataFlowNodes
}  // namespace FN
