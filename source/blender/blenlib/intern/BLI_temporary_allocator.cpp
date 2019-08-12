#include "BLI_temporary_allocator.hpp"
#include "BLI_stack.hpp"

namespace BLI {

constexpr uint SMALL_BUFFER_SIZE = 64 * 1024;

struct ThreadLocalBuffers {
  Stack<void *> buffers;

  ~ThreadLocalBuffers()
  {
    for (void *ptr : buffers) {
      MEM_freeN(ptr);
    }
  }
};

thread_local ThreadLocalBuffers local_buffers;

void *allocate_temp_buffer(uint size)
{
  BLI_assert(size <= SMALL_BUFFER_SIZE);
  UNUSED_VARS_NDEBUG(size);

  Stack<void *> &buffers = local_buffers.buffers;
  if (buffers.empty()) {
    return MEM_mallocN_aligned(SMALL_BUFFER_SIZE, 64, __func__);
  }
  else {
    return buffers.pop();
  }
}

void free_temp_buffer(void *buffer)
{
  Stack<void *> &buffers = local_buffers.buffers;
  buffers.push(buffer);
}

}  // namespace BLI
