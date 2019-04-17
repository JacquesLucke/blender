#ifndef __FUNCTIONS_TYPES_WRAPPER_C_H__
#define __FUNCTIONS_TYPES_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnFloatList *FnFloatList;
typedef struct OpaqueFnFVec3List *FnFVec3List;

const char *FN_type_name(FnType type);
void FN_type_free(FnType type);

#define TYPE_GET_AND_BORROW(name) \
  FnType FN_type_get_##name(void); \
  FnType FN_type_borrow_##name(void);

TYPE_GET_AND_BORROW(float);
TYPE_GET_AND_BORROW(int32);
TYPE_GET_AND_BORROW(fvec3);
TYPE_GET_AND_BORROW(float_list);
TYPE_GET_AND_BORROW(fvec3_list);
#undef TYPE_GET_AND_BORROW

#define LIST_TYPE(name, ptr_type, list_type) \
  uint FN_list_size_##name(list_type list); \
  ptr_type FN_list_data_##name(list_type list); \
  void FN_list_free_##name(list_type list);

LIST_TYPE(float, float *, FnFloatList);
LIST_TYPE(fvec3, float *, FnFVec3List);
#undef LIST_TYPE

#ifdef __cplusplus
}

#  include "lists.hpp"
#  include "numeric.hpp"

WRAPPERS(FN::Types::List<float> *, FnFloatList);
WRAPPERS(FN::Types::List<FN::Types::Vector> *, FnFVec3List);

#endif /* __cplusplus */

#endif /* __FUNCTIONS_TYPES_WRAPPER_C_H__ */
