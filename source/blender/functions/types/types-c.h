#ifndef __FUNCTIONS_TYPES_WRAPPER_C_H__
#define __FUNCTIONS_TYPES_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_GET(name) FnType FN_type_get_##name(void);

TYPE_GET(float);
TYPE_GET(int32);
TYPE_GET(float3);
TYPE_GET(float_list);
TYPE_GET(float3_list);

#undef TYPE_GET

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_TYPES_WRAPPER_C_H__ */
