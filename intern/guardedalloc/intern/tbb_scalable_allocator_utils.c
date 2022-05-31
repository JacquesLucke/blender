#include "C:/blender-git/lib/win64_vc15/tbb/include/tbb/scalable_allocator.h"

#include "MEM_guardedalloc.h"
#include "mallocn_intern.h"

#define ALIGN_THRESHOLD 8

void *tbb_malloc(const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    return scalable_malloc(size);
  }
  return scalable_aligned_malloc(size, alignment);
}