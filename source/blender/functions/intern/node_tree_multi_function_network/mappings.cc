#include "mappings.h"

#include "BLI_lazy_init_cxx.h"
#include "BLI_math_cxx.h"

#include "FN_multi_functions.h"

namespace FN {
namespace MFGeneration {

BLI_LAZY_INIT_REF(const FunctionTreeMFMappings, get_function_tree_multi_function_mappings)
{
  auto mappings = BLI::make_unique<FunctionTreeMFMappings>();
  add_function_tree_socket_mapping_info(*mappings);
  add_function_tree_node_mapping_info(*mappings);
  return mappings;
}

}  // namespace MFGeneration
}  // namespace FN
