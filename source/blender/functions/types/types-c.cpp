#include "FN_types.hpp"

using namespace FN;

const char *FN_type_name(FnType type)
{
  return unwrap(type)->name().data();
}

void FN_type_free(FnType type)
{
  unwrap(type)->decref();
}

static FnType get_type_with_increased_refcount(const SharedType &type)
{
  Type *typeref = type.ptr();
  typeref->incref();
  return wrap(typeref);
}

#define SIMPLE_TYPE_GETTER(name) \
  FnType FN_type_get_##name() \
  { \
    return get_type_with_increased_refcount(Types::GET_TYPE_##name()); \
  } \
  FnType FN_type_borrow_##name() \
  { \
    return wrap(Types::GET_TYPE_##name().ptr()); \
  }

SIMPLE_TYPE_GETTER(float);
SIMPLE_TYPE_GETTER(int32);
SIMPLE_TYPE_GETTER(float3);
SIMPLE_TYPE_GETTER(float_list);
SIMPLE_TYPE_GETTER(float3_list);

uint FN_generic_list_size(FnGenericList list_c)
{
  GenericList *list = unwrap(list_c);
  return list->size();
}

void *FN_generic_list_storage(FnGenericList list_c)
{
  GenericList *list = unwrap(list_c);
  return list->storage();
}

void FN_generic_list_free(FnGenericList list_c)
{
  GenericList *list = unwrap(list_c);
  list->decref();
}
