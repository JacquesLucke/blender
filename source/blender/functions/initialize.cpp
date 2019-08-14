#include "FN_all.hpp"

void FN_initialize()
{
  FN::initialize_llvm();
  FN::Types::initialize_types();
}

void FN_exit()
{
  FN::Types::uninitialize_types();
}
