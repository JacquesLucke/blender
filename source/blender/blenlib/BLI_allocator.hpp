#pragma once

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"
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
 private:
  struct MemHead {
    int offset;
  };

 public:
  void *allocate(uint size, const char *UNUSED(name))
  {
    void *ptr = malloc(size + sizeof(MemHead));
    ((MemHead *)ptr)->offset = sizeof(MemHead);
    return POINTER_OFFSET(ptr, sizeof(MemHead));
  }

  void *allocate_aligned(uint size, uint alignment, const char *UNUSED(name))
  {
    BLI_assert(is_power_of_2_i(alignment));
    void *ptr = malloc(size + alignment + sizeof(MemHead));
    void *used_ptr = (void *)((uintptr_t)POINTER_OFFSET(ptr, alignment + sizeof(MemHead)) &
                              ~((uintptr_t)alignment - 1));
    uint offset = (uintptr_t)used_ptr - (uintptr_t)ptr;
    BLI_assert(offset >= sizeof(MemHead));
    ((MemHead *)used_ptr - 1)->offset = offset;
    return used_ptr;
  }

  void deallocate(void *ptr)
  {
    MemHead *head = (MemHead *)ptr - 1;
    int offset = -head->offset;
    void *actual_pointer = POINTER_OFFSET(ptr, offset);
    free(actual_pointer);
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
