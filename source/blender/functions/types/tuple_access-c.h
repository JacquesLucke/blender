#ifndef __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__
#define __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__

#include "FN_cpp-c.h"
#include "types-c.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_int32(FnTuple tuple, uint index, int32_t value);
void FN_tuple_set_float3(FnTuple tuple, uint index, float vector[3]);
float FN_tuple_get_float(FnTuple tuple, uint index);
int32_t FN_tuple_get_int32(FnTuple tuple, uint index);
void FN_tuple_get_float3(FnTuple tuple, uint index, float dst[3]);
FnList FN_tuple_relocate_out_list(FnTuple tuple, uint index);

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_TYPES_TUPLE_ACCESS_C_H__ */
