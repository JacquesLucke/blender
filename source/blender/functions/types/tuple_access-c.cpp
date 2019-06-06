#include "FN_types.hpp"

using namespace FN;
using namespace FN::Types;

void FN_tuple_set_float(FnTuple tuple_c, uint index, float value)
{
  unwrap(tuple_c)->set<float>(index, value);
}

float FN_tuple_get_float(FnTuple tuple_c, uint index)
{
  return unwrap(tuple_c)->get<float>(index);
}

void FN_tuple_set_int32(FnTuple tuple_c, uint index, int32_t value)
{
  unwrap(tuple_c)->set<int32_t>(index, value);
}

int32_t FN_tuple_get_int32(FnTuple tuple_c, uint index)
{
  return unwrap(tuple_c)->get<int32_t>(index);
}

void FN_tuple_set_fvec3(FnTuple tuple_c, uint index, float value[3])
{
  unwrap(tuple_c)->set<Vector>(index, *(Vector *)value);
}

void FN_tuple_get_fvec3(FnTuple tuple_c, uint index, float dst[3])
{
  *(Vector *)dst = unwrap(tuple_c)->get<Vector>(index);
}

FnFloatList FN_tuple_relocate_out_float_list(FnTuple tuple_c, uint index)
{
  auto list = unwrap(tuple_c)->relocate_out<SharedFloatList>(index);
  return wrap(list.extract_ptr());
}

FnFVec3List FN_tuple_relocate_out_fvec3_list(FnTuple tuple_c, uint index)
{
  auto list = unwrap(tuple_c)->relocate_out<SharedFVec3List>(index);
  return wrap(list.extract_ptr());
}
