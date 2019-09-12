/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <mutex>

#include "BLI_lazy_init_cxx.h"
#include "BLI_stack_cxx.h"

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
