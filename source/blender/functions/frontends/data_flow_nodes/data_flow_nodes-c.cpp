#include "FN_data_flow_nodes.hpp"
#include "BLI_timeit.h"

using namespace FN;
using BLI::Optional;

FnFunction FN_tree_to_function(bNodeTree *btree)
{
  SCOPED_TIMER("Tree to function");
  BLI_assert(btree);
  Optional<std::unique_ptr<Function>> optional_fn = DataFlowNodes::generate_function(btree);
  if (!optional_fn.has_value()) {
    return nullptr;
  }

  std::unique_ptr<Function> fn = optional_fn.extract();
  return wrap(fn.release());
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
