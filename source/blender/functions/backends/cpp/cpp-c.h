#ifndef __FUNCTIONS_TUPLE_WRAPPER_C_H__
#define __FUNCTIONS_TUPLE_WRAPPER_C_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnTuple *FnTuple;
typedef struct OpaqueFnList *FnList;

void FN_tuple_free(FnTuple tuple);
void fn_tuple_destruct(FnTuple tuple);

uint FN_list_size(FnList list);
void *FN_list_storage(FnList list);
void FN_list_free(FnList list);

FnList FN_tuple_relocate_out_list(FnTuple tuple, uint index);

#ifdef __cplusplus
}

#  include "tuple.hpp"
#  include "list.hpp"

WRAPPERS(FN::Tuple *, FnTuple);
WRAPPERS(FN::List *, FnList);

#endif /* __cplusplus */

#endif /* __FUNCTIONS_TUPLE_WRAPPER_C_H__ */
