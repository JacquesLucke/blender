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
#include <stack>

#include "BLI_temporary_allocator.h"
#include "BLI_stack.hpp"

using namespace BLI;

constexpr uint SMALL_BUFFER_SIZE = 64 * 1024;

struct ThreadLocalBuffers {
  Stack<void *, 32, RawAllocator> buffers;

  ~ThreadLocalBuffers()
  {
    for (void *ptr : buffers) {
      RawAllocator().deallocate(ptr);
    }
  }
};

thread_local ThreadLocalBuffers local_storage;

void *BLI_temporary_allocate(uint size)
{
  BLI_assert(size <= SMALL_BUFFER_SIZE);
  UNUSED_VARS_NDEBUG(size);

  auto &buffers = local_storage.buffers;
  if (buffers.empty()) {
    void *ptr = RawAllocator().allocate_aligned(SMALL_BUFFER_SIZE, 64, __func__);
    return ptr;
  }
  else {
    return buffers.pop();
  }
}

void BLI_temporary_deallocate(void *buffer)
{
  auto &buffers = local_storage.buffers;
  buffers.push(buffer);
}
