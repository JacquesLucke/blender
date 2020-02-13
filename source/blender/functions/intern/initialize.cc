#include "FN_initialize.h"
#include "cpp_types.h"
#include "node_tree_multi_function_network/mappings.h"

void FN_initialize(void)
{
  FN::init_cpp_types();
  FN::MFGeneration::init_function_tree_mf_mappings();
}

void FN_exit(void)
{
  FN::MFGeneration::free_function_tree_mf_mappings();
  FN::free_cpp_types();
}
