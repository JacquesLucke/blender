#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "DNA_node_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void FN_initialize(void);

typedef struct OpaqueFnFunction *FnFunction;
typedef struct OpaqueFnType *FnType;
typedef struct OpaqueFnTuple *FnTuple;
typedef struct OpaqueFnTupleCallBody *FnTupleCallBody;

/************** Core *************/

void FN_function_free(FnFunction fn);

bool FN_function_has_signature(FnFunction, FnType *inputs, FnType *outputs);
uint FN_input_amount(FnFunction fn);
uint FN_output_amount(FnFunction fn);
bool FN_input_has_type(FnFunction fn, uint index, FnType type);
bool FN_output_has_type(FnFunction fn, uint index, FnType type);

void FN_function_print(FnFunction fn);


/************** Types *************/

const char *FN_type_name(FnType type);
void FN_type_free(FnType type);

FnType FN_type_get_float(void);
FnType FN_type_get_int32(void);
FnType FN_type_get_fvec3(void);
FnType FN_type_get_float_list(void);

FnType FN_type_borrow_float(void);
FnType FN_type_borrow_int32(void);
FnType FN_type_borrow_fvec3(void);
FnType FN_type_borrow_float_list(void);


/*************** Tuple Call ****************/

FnTupleCallBody FN_tuple_call_get(FnFunction fn);
void FN_tuple_call_invoke(FnTupleCallBody body, FnTuple fn_in, FnTuple fn_out);
FnTuple FN_tuple_for_input(FnTupleCallBody body);
FnTuple FN_tuple_for_output(FnTupleCallBody body);

void FN_tuple_free(FnTuple tuple);

void FN_tuple_set_float(FnTuple tuple, uint index, float value);
void FN_tuple_set_int32(FnTuple tuple, uint index, int32_t value);
void FN_tuple_set_float_vector_3(FnTuple tuple, uint index, float vector[3]);
float FN_tuple_get_float(FnTuple tuple, uint index);
int32_t FN_tuple_get_int32(FnTuple tuple, uint index);
void FN_tuple_get_float_vector_3(FnTuple tuple, uint index, float dst[3]);

uint fn_tuple_stack_prepare_size(FnTupleCallBody body);
void fn_tuple_prepare_stack(
	FnTupleCallBody body,
	void *buffer,
	FnTuple *fn_in,
	FnTuple *fn_out);

void fn_tuple_destruct(FnTuple tuple);

#define FN_TUPLE_CALL_PREPARE_STACK(body, fn_in, fn_out) \
	FnTuple fn_in, fn_out; \
	void *fn_in##_##fn_out##_buffer = alloca(fn_tuple_stack_prepare_size(body)); \
	fn_tuple_prepare_stack(body, fn_in##_##fn_out##_buffer, &fn_in, &fn_out);

#define FN_TUPLE_CALL_DESTRUCT_STACK(body, fn_in, fn_out) \
	fn_tuple_destruct(fn_in); \
	fn_tuple_destruct(fn_out);

#define FN_TUPLE_CALL_PREPARE_HEAP(body, fn_in, fn_out) \
	FnTuple fn_in = FN_tuple_for_input(body); \
	FnTuple fn_out = FN_tuple_for_output(body); \

#define FN_TUPLE_CALL_DESTRUCT_HEAP(body, fn_in, fn_out) \
	FN_tuple_free(fn_in); \
	FN_tuple_free(fn_out);

/*************** Dependencies ****************/

struct DepsNodeHandle;
void FN_function_update_dependencies(
	FnFunction fn,
	struct DepsNodeHandle *deps_node);


/************ Data Flow Nodes ****************/

FnFunction FN_tree_to_function(bNodeTree *bnodetree);
FnFunction FN_function_get_with_signature(
	bNodeTree *btree, FnType *inputs, FnType *outputs);


#ifdef __cplusplus
}
#endif

#endif  /* __FUNCTIONS_H__ */
