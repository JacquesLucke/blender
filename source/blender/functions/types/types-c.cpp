#include "FN_types.hpp"

using namespace FN;

static FnType get_type_with_increased_refcount(Type *type)
{
  return wrap(type);
}

#define SIMPLE_TYPE_GETTER(name) \
  FnType FN_type_get_##name() \
  { \
    return get_type_with_increased_refcount(Types::GET_TYPE_##name()); \
  }

SIMPLE_TYPE_GETTER(float);
SIMPLE_TYPE_GETTER(int32);
SIMPLE_TYPE_GETTER(float3);
SIMPLE_TYPE_GETTER(float_list);
SIMPLE_TYPE_GETTER(float3_list);
