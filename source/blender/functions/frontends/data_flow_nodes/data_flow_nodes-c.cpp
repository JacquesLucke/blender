#include "FN_data_flow_nodes.hpp"
#include "BLI_timeit.hpp"

using namespace FN;

FnFunction FN_tree_to_function(bNodeTree *btree)
{
  TIMEIT("Tree to function");
  BLI_assert(btree);
  auto fn_opt = DataFlowNodes::generate_function(btree);
  if (!fn_opt.has_value()) {
    return nullptr;
  }

  Function *fn_ptr = fn_opt.value().ptr();
  fn_ptr->incref();
  return wrap(fn_ptr);
}

FnFunction FN_function_get_with_signature(bNodeTree *btree, FnType *inputs, FnType *outputs)
{
  if (btree == NULL) {
    return NULL;
  }

  FnFunction fn = FN_tree_to_function(btree);
  if (fn == NULL) {
    return NULL;
  }
  else if (FN_function_has_signature(fn, inputs, outputs)) {
    return fn;
  }
  else {
    FN_function_free(fn);
    return NULL;
  }
}
