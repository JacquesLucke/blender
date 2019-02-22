#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"
#include "DNA_node_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void FN_initialize(void);

typedef struct OpaqueFnFunction *FnFunction;
typedef struct OpaqueFnType *FnType;
typedef struct OpaqueFnTuple *FnTuple;
typedef struct OpaqueFnTupleCallBody *FnTupleCallBody;

FnTupleCallBody FN_function_get_callable(FnFunction fn);
void FN_function_call(FnTupleCallBody call, FnTuple fn_in, FnTuple fn_out);
void FN_function_free(FnFunction fn);

bool FN_function_has_signature(FnFunction, FnType *inputs, FnType *outputs);
uint FN_input_amount(FnFunction fn);
uint FN_output_amount(FnFunction fn);
bool FN_input_has_type(FnFunction fn, uint index, FnType type);
bool FN_output_has_type(FnFunction fn, uint index, FnType type);

void FN_function_print(FnFunction fn);

FnTuple FN_tuple_for_input(FnFunction fn);
FnTuple FN_tuple_for_output(FnFunction fn);

void FN_tuple_free(FnTuple tuple);

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_float_vector_3(FnTuple tuple, uint index, float vector[3]);
float FN_tuple_get_float(FnTuple tuple, uint index);
void FN_tuple_get_float_vector_3(FnTuple tuple, uint index, float dst[3]);

const char *FN_type_name(FnType type);
void FN_type_free(FnType type);

FnType FN_type_get_float(void);
FnType FN_type_get_int32(void);
FnType FN_type_get_fvec3(void);

FnType FN_type_borrow_float(void);
FnType FN_type_borrow_int32(void);
FnType FN_type_borrow_fvec3(void);

FnFunction FN_tree_to_function(bNodeTree *bnodetree);
FnFunction FN_function_get_with_signature(
	bNodeTree *btree, FnType *inputs, FnType *outputs);

struct DepsNodeHandle;
void FN_function_update_dependencies(
	FnFunction fn,
	struct DepsNodeHandle *deps_node);

#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
