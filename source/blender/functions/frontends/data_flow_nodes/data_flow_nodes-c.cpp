#include "FN_data_flow_nodes.hpp"
#include "BLI_timeit.h"

using namespace FN;

FnFunction FN_tree_to_function(bNodeTree *btree)
{
  SCOPED_TIMER("Tree to function");
  BLI_assert(btree);
  ValueOrError<SharedFunction> fn_or_error = DataFlowNodes::generate_function(btree);
  if (fn_or_error.is_error()) {
    return nullptr;
  }

  SharedFunction fn = fn_or_error.extract_value();
  Function *fn_ptr = fn.ptr();
  fn_ptr->incref();
  return wrap(fn_ptr);
}

FnFunction FN_function_get_with_signature(bNodeTree *btree, FnType *inputs_c, FnType *outputs_c)
{
  if (btree == NULL) {
    return NULL;
  }

  FnFunction fn = FN_tree_to_function(btree);
  if (fn == NULL) {
    return NULL;
  }
  else if (FN_function_has_signature(fn, inputs_c, outputs_c)) {
    return fn;
  }
  else {
    FN_function_free(fn);
    return NULL;
  }
}
