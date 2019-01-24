#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFunction *FunctionRef;
typedef struct OpaqueFnType *FnTypeRef;
typedef struct OpaqueFnInputs *FnInputsRef;
typedef struct OpaqueFnOutputs *FnOutputsRef;

/* Call a function with the given input.
 * The function output will be written into fn_out.
 * Returns true on success. */
bool FN_function_call(FunctionRef fn, FnInputsRef fn_in, FnOutputsRef fn_out);


/* Create a container to store function inputs. */
FnInputsRef FN_inputs_new(FunctionRef fn);

/* Free a set of function inputs. */
void FN_inputs_free(FnInputsRef fn_in);

/* Set a function input by index. Returns true on success. */
void FN_inputs_set(FnInputsRef fn_in, uint index, void *src);
void FN_inputs_set_float(FnInputsRef fn_in, uint index, float value);
void FN_inputs_set_float_vector_3(FnInputsRef fn_in, uint index, float vector[3]);

/* Create a container to store function outputs. */
FnOutputsRef FN_outputs_new(FunctionRef fn);

/* Free a set of output functions. */
void FN_outputs_free(FnOutputsRef fn_out);

/* Extract the result of an executed function by index. */
void FN_outputs_get(FnOutputsRef fn_out, uint index, void *dst);
void FN_outputs_get_float_vector_3(FnOutputsRef fn_out, uint index, float dst[3]);

const char *FN_type_name(FnTypeRef type);

FnTypeRef FN_type_get_float(void);
FnTypeRef FN_type_get_int32(void);
FnTypeRef FN_type_get_float_vector_3d(void);

FunctionRef FN_get_add_const_function(int value);
FunctionRef FN_get_deform_function(void);

#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
