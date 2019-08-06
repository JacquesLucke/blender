#ifndef __FUNCTIONS_TYPES_WRAPPER_C_H__
#define __FUNCTIONS_TYPES_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_GET_AND_BORROW(name) \
  FnType FN_type_get_##name(void); \
  FnType FN_type_borrow_##name(void);

TYPE_GET_AND_BORROW(float);
TYPE_GET_AND_BORROW(int32);
TYPE_GET_AND_BORROW(float3);
TYPE_GET_AND_BORROW(float_list);
TYPE_GET_AND_BORROW(float3_list);
#undef TYPE_GET_AND_BORROW

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_TYPES_WRAPPER_C_H__ */
