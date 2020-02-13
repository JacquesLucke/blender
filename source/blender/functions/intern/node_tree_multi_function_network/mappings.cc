#include "mappings.h"

#include "BLI_math_cxx.h"

#include "FN_multi_functions.h"

namespace FN {
namespace MFGeneration {

static FunctionTreeMFMappings *mappings;

void init_function_tree_mf_mappings()
{
  mappings = new FunctionTreeMFMappings();
  add_function_tree_socket_mapping_info(*mappings);
  add_function_tree_node_mapping_info(*mappings);
}

void free_function_tree_mf_mappings()
{
  delete mappings;
}

const FunctionTreeMFMappings &get_function_tree_multi_function_mappings()
{
  return *mappings;
}

}  // namespace MFGeneration
}  // namespace FN
