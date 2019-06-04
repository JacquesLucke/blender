#include "BLI_lazy_init.hpp"
#include "BLI_lazy_init.h"
#include "BLI_small_stack.hpp"

typedef std::function<void()> FreeFunc;
BLI::SmallStack<FreeFunc> free_functions;

void BLI_lazy_init_free_all()
{
  while (!free_functions.empty()) {
    FreeFunc free_object = free_functions.pop();
    free_object();
  }
  free_functions.clear_and_make_small();
}

namespace BLI {

void register_lazy_init_free_func(std::function<void()> free_func)
{
  free_functions.push(free_func);
}

}  // namespace BLI
