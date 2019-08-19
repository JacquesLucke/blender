#pragma once

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_temporary_allocator.h"

namespace BLI {

class GuardedAllocator {
 public:
  void *allocate(uint size, const char *name)
  {
    return MEM_mallocN(size, name);
  }

  void *allocate_aligned(uint size, uint alignment, const char *name)
  {
    alignment = std::max<uint>(alignment, 8);
    return MEM_mallocN_aligned(size, alignment, name);
  }

  void deallocate(void *ptr)
  {
    MEM_freeN(ptr);
  }
};

class RawAllocator {
 public:
  void *allocate(uint size, const char *UNUSED(name))
  {
    return malloc(size);
  }

  void *allocate_aligned(uint size, uint alignment, const char *UNUSED(name))
  {
    return aligned_alloc(alignment, size);
  }

  void deallocate(void *ptr)
  {
    free(ptr);
  }
};

class TemporaryAllocator {
 public:
  void *allocate(uint size, const char *UNUSED(name))
  {
    return BLI_temporary_allocate(size);
  }

  void *allocate_aligned(uint size, uint alignment, const char *UNUSED(name))
  {
    BLI_assert(alignment <= 64);
    UNUSED_VARS_NDEBUG(alignment);
    return BLI_temporary_allocate(size);
  }

  void deallocate(void *ptr)
  {
    BLI_temporary_deallocate(ptr);
  }
};

}  // namespace BLI
