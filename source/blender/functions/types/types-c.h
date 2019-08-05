#ifndef __FUNCTIONS_TYPES_WRAPPER_C_H__
#define __FUNCTIONS_TYPES_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnGenericList *FnGenericList;

const char *FN_type_name(FnType type);
void FN_type_free(FnType type);

#define TYPE_GET_AND_BORROW(name) \
  FnType FN_type_get_##name(void); \
  FnType FN_type_borrow_##name(void);

TYPE_GET_AND_BORROW(float);
TYPE_GET_AND_BORROW(int32);
TYPE_GET_AND_BORROW(float3);
TYPE_GET_AND_BORROW(float_list);
TYPE_GET_AND_BORROW(float3_list);
#undef TYPE_GET_AND_BORROW

uint FN_generic_list_size(FnGenericList list);
void *FN_generic_list_storage(FnGenericList list);
void FN_generic_list_free(FnGenericList list);

#ifdef __cplusplus
}

#  include "lists.hpp"
#  include "numeric.hpp"

WRAPPERS(FN::GenericList *, FnGenericList);

#endif /* __cplusplus */

#endif /* __FUNCTIONS_TYPES_WRAPPER_C_H__ */
