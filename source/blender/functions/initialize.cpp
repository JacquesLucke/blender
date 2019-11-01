#include "FN_all.hpp"

void FN_old_initialize()
{
  FN::initialize_llvm();
  FN::Types::initialize_types();
}

void FN_old_exit()
{
  FN::Types::uninitialize_types();
}
