#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnFunction *FnFunction;
typedef struct OpaqueFnType *FnType;
typedef struct OpaqueFnTuple *FnTuple;
typedef struct OpaqueFnCallable *FnCallable;

FnCallable FN_function_get_callable(FnFunction fn);
void FN_function_call(FnCallable call, FnTuple fn_in, FnTuple fn_out);
void FN_function_free(FnFunction fn);

FnTuple FN_tuple_for_input(FnFunction fn);
FnTuple FN_tuple_for_output(FnFunction fn);

void FN_tuple_free(FnTuple tuple);

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_float_vector_3(FnTuple tuple, uint index, float vector[3]);

void FN_tuple_get_float_vector_3(FnTuple tuple, uint index, float dst[3]);

const char *FN_type_name(FnType type);
void FN_type_free(FnType type);

FnType FN_type_get_float(void);
FnType FN_type_get_int32(void);
FnType FN_type_get_fvec3(void);

FnFunction FN_get_deform_function(int type);

#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
