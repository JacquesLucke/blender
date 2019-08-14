#include "FN_all.hpp"

void FN_initialize()
{
  FN::initialize_llvm();
  FN::Types::initialize_types();
}
