#include <string.h>

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

void *tbb_calloc(const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    return scalable_calloc(1, size);
  }
  void *ptr = scalable_aligned_malloc(size, alignment);
  memset(ptr, 0, size);
  return ptr;
}

void *tbb_realloc(void *ptr,
                  const size_t new_size,
                  const size_t new_alignment,
                  const size_t old_size,
                  const size_t old_alignment)
{
  const bool new_alignment_is_small = new_alignment <= ALIGN_THRESHOLD;
  const bool old_alignment_is_small = old_alignment <= ALIGN_THRESHOLD;
  if (new_alignment_is_small && old_alignment_is_small) {
    return scalable_realloc(ptr, new_size);
  }
  if (!new_alignment_is_small && !old_alignment_is_small) {
    return scalable_aligned_realloc(ptr, new_size, new_alignment);
  }
  /* Only one of the old or new alignment is large, so the reallocation has to be done manually. */
  void *new_ptr = tbb_malloc(new_size, new_alignment);
  if (ptr == NULL) {
    return new_ptr;
  }
  const size_t bytes_to_copy = new_size < old_size ? new_size : old_size;
  memcpy(new_ptr, ptr, bytes_to_copy);
  tbb_free(ptr, old_alignment);
  return new_ptr;
}

void tbb_free(void *ptr, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    scalable_free(ptr);
  }
  else {
    scalable_aligned_free(ptr);
  }
}

size_t tbb_real_size(const void *ptr)
{
  return scalable_msize((void *)ptr);
}
