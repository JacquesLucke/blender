#ifndef __FUNCTIONS_TUPLE_WRAPPER_C_H__
#define __FUNCTIONS_TUPLE_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnTuple *FnTuple;

void FN_tuple_free(FnTuple tuple);

void fn_tuple_destruct(FnTuple tuple);

#ifdef __cplusplus
}

#  include "tuple.hpp"

WRAPPERS(FN::Tuple *, FnTuple);

#endif /* __cplusplus */

#endif /* __FUNCTIONS_TUPLE_WRAPPER_C_H__ */
