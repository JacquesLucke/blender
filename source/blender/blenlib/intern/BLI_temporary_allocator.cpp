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
