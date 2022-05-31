/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "jemalloc.h"

#include "MEM_guardedalloc.h"
#include "mallocn_intern.h"

#define ALIGN_THRESHOLD 8

void *jemalloc_malloc(const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    return malloc(size);
  }
  return mallocx(size, MALLOCX_ALIGN(alignment));
}

void *jemalloc_calloc(const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    return calloc(1, size);
  }
  return mallocx(size, MALLOCX_ALIGN(alignment) | MALLOCX_ZERO);
}

void *jemalloc_realloc(void *ptr, const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    return realloc(ptr, size);
  }
  return rallocx(ptr, size, MALLOCX_ALIGN(alignment));
}

void jemalloc_free(void *ptr)
{
  free(ptr);
}

size_t jemalloc_real_size(const size_t size, const size_t alignment)
{
  if (alignment <= ALIGN_THRESHOLD) {
    /* Prefer passing no flag in case it is more optimized. */
    return nallocx(size, 0);
  }
  return nallocx(size, MALLOCX_ALIGN(alignment));
}
