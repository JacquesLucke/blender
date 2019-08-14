#ifndef __FUNCTIONS_TYPES_WRAPPER_C_H__
#define __FUNCTIONS_TYPES_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_GETTERS(name) \
  FnType FN_type_get_##name(void); \
  FnType FN_type_get_##name##_list(void);

TYPE_GETTERS(float);
TYPE_GETTERS(int32);
TYPE_GETTERS(float3);

#undef TYPE_GETTERS

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_TYPES_WRAPPER_C_H__ */
