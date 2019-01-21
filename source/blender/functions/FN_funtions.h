#include "BLI_utildefines.h"

struct Function;
typedef struct Function Function;

struct FnInputs;
typedef struct FnInputs FnInputs;

struct FnOutputs;
typedef struct FnOutputs FnOutputs;

struct FnStaticInputs;
typedef struct FnStaticInputs FnStaticInputs;

struct FnDependencies;
typedef struct FnDependencies FnDependencies;

/* Split ownership of the function. */
void FN_function_copy_ref(Function *fn);

/* Tag the function as unused by the caller. */
void FN_function_free_ref(Function *fn);


/* Raw function pointer to call when the function should be executed. */
void *FN_function_get_pointer(Function *fn);

/* Pass into the function as first argument. */
void *FN_function_get_settings(Function *fn);

/* Call a function with the given input.
 * The function output will be written into fn_out.
 * Returns true on success. */
bool FN_function_call(Function *fn, FnInputs *fn_in, FnOutputs *fn_out);


/* Create a container to store function inputs. */
FnInputs *FN_inputs_new(Function *fn);

/* Free a set of function inputs. */
void FN_inputs_free(FnInputs *fn_in);

/* Set a funtion input by name. Returns true on success. */
bool FN_inputs_set_name(FnInputs *fn_in, const char *name, void *value);

/* Set a function input by index. Returns true on success. */
bool FN_inputs_set_index(FnInputs *fn_in, uint index, void *value);


/* Create a container to store function outputs. */
FnOutputs *FN_outputs_new(Function *fn);

/* Free a set of output functions. */
void FN_outputs_free(FnOutputs *fn_out);

/* Extract the result of an executed function by name. */
void *FN_outputs_get_name(FnOutputs *fn_out, const char *name);

/* Extract the result of an executed function by index. */
void *FN_outputs_get_index(FnOutputs *fn_out, const char *name);


/* Get dependencies of function given some static inputs.
 * Returns NULL on failure (when not all static inputs are given). */
FnDependencies *FN_dependencies_get(Function *fn, FnStaticInputs *fn_in);
