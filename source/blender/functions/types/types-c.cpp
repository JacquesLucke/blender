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
SIMPLE_TYPE_GETTER(fvec3);
SIMPLE_TYPE_GETTER(float_list);
SIMPLE_TYPE_GETTER(fvec3_list);

#define LIST_WRAPPER(name, ptr_type, list_type) \
  uint FN_list_size_##name(list_type list) \
  { \
    return unwrap(list)->size(); \
  } \
  ptr_type FN_list_data_##name(list_type list) \
  { \
    return (ptr_type)unwrap(list)->data_ptr(); \
  } \
  void FN_list_free_##name(list_type list) \
  { \
    unwrap(list)->remove_user(); \
  }

LIST_WRAPPER(float, float *, FnFloatList);
LIST_WRAPPER(fvec3, float *, FnFVec3List);
