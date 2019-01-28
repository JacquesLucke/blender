#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueCPUFunction *FnCPUFunction;
typedef struct OpaqueFnType *FnTypeRef;
typedef struct OpaqueFnTuple *FnTuple;

void FN_function_call(FnCPUFunction fn, FnTuple fn_in, FnTuple fn_out);

FnTuple FN_tuple_for_input(FnCPUFunction fn);
FnTuple FN_tuple_for_output(FnCPUFunction fn);

void FN_tuple_free(FnTuple tuple);

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_float_vector_3(FnTuple tuple, uint index, float vector[3]);

void FN_tuple_get_float_vector_3(FnTuple tuple, uint index, float dst[3]);

const char *FN_type_name(FnTypeRef type);

FnTypeRef FN_type_get_float(void);
FnTypeRef FN_type_get_int32(void);
FnTypeRef FN_type_get_float_vector_3d(void);

FnCPUFunction FN_get_deform_function(void);

#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
