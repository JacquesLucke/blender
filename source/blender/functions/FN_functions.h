#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Function *FunctionRef;
typedef struct FnType *FnTypeRef;
typedef struct FnInputs *FnInputsRef;
typedef struct FnOutputs *FnOutputsRef;

/* Split ownership of the function. */
void FN_function_copy_ref(FunctionRef fn);

/* Tag the function as unused by the caller. */
void FN_function_free_ref(FunctionRef fn);


/* Raw function pointer to call when the function should be executed. */
void *FN_function_get_pointer(FunctionRef fn);

/* Pass into the function as first argument. */
void *FN_function_get_settings(FunctionRef fn);

/* Call a function with the given input.
 * The function output will be written into fn_out.
 * Returns true on success. */
bool FN_function_call(FunctionRef fn, FnInputsRef fn_in, FnOutputsRef fn_out);


/* Create a container to store function inputs. */
FnInputsRef FN_inputs_new(FunctionRef fn);

/* Free a set of function inputs. */
void FN_inputs_free(FnInputsRef fn_in);

/* Set a funtion input by name. Returns true on success. */
bool FN_inputs_set_name(FnInputsRef fn_in, const char *name, void *value);

/* Set a function input by index. Returns true on success. */
void FN_inputs_set_index(FnInputsRef fn_in, uint index, void *value);


/* Create a container to store function outputs. */
FnOutputsRef FN_outputs_new(FunctionRef fn);

/* Free a set of output functions. */
void FN_outputs_free(FnOutputsRef fn_out);

/* Extract the result of an executed function by name. */
void *FN_outputs_get_name(FnOutputsRef fn_out, const char *name);

/* Extract the result of an executed function by index. */
void *FN_outputs_get_index(FnOutputsRef fn_out, const char *name);

const char *FN_type_name(FnTypeRef type);

FnTypeRef FN_type_get_float(void);
FnTypeRef FN_type_get_int32(void);
FnTypeRef FN_type_get_float_vector_3d(void);

FunctionRef FN_get_add_const_function(int value);

#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
