#include "FN_types.hpp"

using namespace FN;

#define SIMPLE_TYPE_GETTER(name) \
  FnType FN_type_get_##name() \
  { \
    return wrap(Types::TYPE_##name); \
  } \
  FnType FN_type_get_##name##_list() \
  { \
    return wrap(Types::TYPE_##name##_list); \
  }

SIMPLE_TYPE_GETTER(float);
SIMPLE_TYPE_GETTER(int32);
SIMPLE_TYPE_GETTER(float3);
