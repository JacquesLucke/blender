#ifndef __FUNCTIONS_DEPENDENCIES_C_WRAPPER_H__
#define __FUNCTIONS_DEPENDENCIES_C_WRAPPER_H__

#include "FN_core-c.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DepsNodeHandle;
void FN_function_update_dependencies(
	FnFunction fn,
	struct DepsNodeHandle *deps_node);

#ifdef __cplusplus
}
#endif

#endif /* __FUNCTIONS_DEPENDENCIES_C_WRAPPER_H__ */