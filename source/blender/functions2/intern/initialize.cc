#include "FN_initialize.h"
#include "cpp_types.h"

void FN_initialize(void)
{
  FN::init_cpp_types();
}

void FN_exit(void)
{
  FN::free_cpp_types();
}
