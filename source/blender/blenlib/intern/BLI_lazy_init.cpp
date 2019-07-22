#include <mutex>

#include "BLI_lazy_init.hpp"
#include "BLI_stack.hpp"

struct FreeFunc {
  std::function<void()> func;
  const char *name;
};

BLI::Stack<FreeFunc> free_functions;
std::mutex store_free_func_mutex;

void BLI_lazy_init_free_all()
{
  while (!free_functions.empty()) {
    FreeFunc free_object = free_functions.pop();
    free_object.func();
  }
  free_functions.clear_and_make_small();
}

void BLI_lazy_init_list_all()
{
  for (FreeFunc &func : free_functions) {
    std::cout << func.name << "\n";
  }
}

namespace BLI {

void lazy_init_register(std::function<void()> free_func, const char *name)
{
  std::lock_guard<std::mutex> lock(store_free_func_mutex);
  free_functions.push({free_func, name});
}

}  // namespace BLI
