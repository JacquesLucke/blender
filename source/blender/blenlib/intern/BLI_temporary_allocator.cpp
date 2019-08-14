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

#include "BLI_temporary_allocator.h"
#include "BLI_temporary_allocator.hpp"
#include "BLI_stack.hpp"

namespace BLI {

constexpr uint SMALL_BUFFER_SIZE = 64 * 1024;

struct ThreadLocalBuffers {
  Stack<void *, 32> buffers;
};

static Vector<void *> all_buffers;
static std::mutex all_buffers_mutex;

thread_local ThreadLocalBuffers local_buffers;

void *allocate_temp_buffer(uint size)
{
  BLI_assert(size <= SMALL_BUFFER_SIZE);
  UNUSED_VARS_NDEBUG(size);

  auto &buffers = local_buffers.buffers;
  if (buffers.empty()) {
    void *ptr = MEM_mallocN_aligned(SMALL_BUFFER_SIZE, 64, __func__);
    std::lock_guard<std::mutex> lock(all_buffers_mutex);
    all_buffers.append(ptr);
    return ptr;
  }
  else {
    return buffers.pop();
  }
}

void free_temp_buffer(void *buffer)
{
  auto &buffers = local_buffers.buffers;
  buffers.push(buffer);
}

}  // namespace BLI

void BLI_temporary_buffers_free_all(void)
{
  for (void *ptr : BLI::all_buffers) {
    MEM_freeN(ptr);
  }
  BLI::all_buffers.clear_and_make_small();
}
