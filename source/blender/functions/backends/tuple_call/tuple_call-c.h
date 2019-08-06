#ifndef __FUNCTIONS_TUPLE_CALL_WRAPPER_C_H__
#define __FUNCTIONS_TUPLE_CALL_WRAPPER_C_H__

#include "FN_core-c.h"
#include "FN_cpp-c.h"
#include "BLI_alloca.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnTupleCallBody *FnTupleCallBody;

FnTupleCallBody FN_tuple_call_get(FnFunction fn);
void FN_tuple_call_invoke(FnTupleCallBody body,
                          FnTuple fn_in,
                          FnTuple fn_out,
                          const char *caller_info);
FnTuple FN_tuple_for_input(FnTupleCallBody body);
FnTuple FN_tuple_for_output(FnTupleCallBody body);

uint fn_tuple_stack_prepare_size(FnTupleCallBody body);
void fn_tuple_prepare_stack(FnTupleCallBody body, void *buffer, FnTuple *fn_in, FnTuple *fn_out);

#define FN_TUPLE_CALL_PREPARE_STACK(body, fn_in, fn_out) \
  FnTuple fn_in, fn_out; \
  void *fn_in##_##fn_out##_buffer = alloca(fn_tuple_stack_prepare_size(body)); \
  fn_tuple_prepare_stack(body, fn_in##_##fn_out##_buffer, &fn_in, &fn_out);

#define FN_TUPLE_CALL_DESTRUCT_STACK(body, fn_in, fn_out) \
  fn_tuple_destruct(fn_in); \
  fn_tuple_destruct(fn_out);

#define FN_TUPLE_CALL_PREPARE_HEAP(body, fn_in, fn_out) \
  FnTuple fn_in = FN_tuple_for_input(body); \
  FnTuple fn_out = FN_tuple_for_output(body);

#define FN_TUPLE_CALL_DESTRUCT_HEAP(body, fn_in, fn_out) \
  FN_tuple_free(fn_in); \
  FN_tuple_free(fn_out);

#ifdef __cplusplus
}

#  include "tuple_call.hpp"

WRAPPERS(FN::TupleCallBody *, FnTupleCallBody);

#endif /* __cplusplus */

#endif /* __FUNCTIONS_TUPLE_CALL_WRAPPER_C_H__ */
