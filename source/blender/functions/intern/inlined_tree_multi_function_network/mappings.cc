#include "mappings.h"

#include "BLI_lazy_init_cxx.h"
#include "BLI_math_cxx.h"

#include "FN_multi_functions.h"

namespace FN {

BLI_LAZY_INIT_REF(const VTreeMultiFunctionMappings, get_inlined_tree_multi_function_mappings)
{
  auto mappings = BLI::make_unique<VTreeMultiFunctionMappings>();
  add_inlined_tree_socket_mapping_info(*mappings);
  add_inlined_tree_node_mapping_info(*mappings);
  return mappings;
}

}  // namespace FN
