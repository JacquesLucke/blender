#include "FN_initialize.h"
#include "cpp_types.h"
#include "node_tree_multi_function_network/mappings.h"
#include "multi_functions/global_functions.h"

void FN_initialize(void)
{
  FN::init_cpp_types();
  FN::init_global_functions();
  FN::MFGeneration::init_function_tree_mf_mappings();
}

void FN_exit(void)
{
  FN::MFGeneration::free_function_tree_mf_mappings();
  FN::free_global_functions();
  FN::free_cpp_types();
}
