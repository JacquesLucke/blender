#ifndef __FUNCTIONS_CORE_WRAPPER_C_H__
#define __FUNCTIONS_CORE_WRAPPER_C_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueFnFunction *FnFunction;
typedef struct OpaqueFnType *FnType;

void FN_function_free(FnFunction fn);

bool FN_function_has_signature(FnFunction, FnType *inputs, FnType *outputs);
uint FN_input_amount(FnFunction fn);
uint FN_output_amount(FnFunction fn);
bool FN_input_has_type(FnFunction fn, uint index, FnType type);
bool FN_output_has_type(FnFunction fn, uint index, FnType type);

void FN_function_print(FnFunction fn);

const char *FN_type_name(FnType type);

#ifdef __cplusplus
}

#  include "type.hpp"
#  include "function.hpp"

#  define WRAPPERS(T1, T2) \
    inline T1 unwrap(T2 value) \
    { \
      return (T1)value; \
    } \
    inline T2 wrap(T1 value) \
    { \
      return (T2)value; \
    }

WRAPPERS(FN::Function *, FnFunction)
WRAPPERS(FN::Type *, FnType)

#endif /* __cplusplus */

#endif /* __FUNCTIONS_CORE_WRAPPER_C_H__ */
