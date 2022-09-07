/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::VectorList` is a dynamically growing ordered container for values of type T.
 * It is *not* guaranteed that all values will be stored in one contiguous array. Instead, multiple
 * arrays may be used.
 *
 * Comparison to `blender::Vector`:
 * - `VectorList` has better performance when appending many elements, because it does not have to
 *   move existing values.
 * - This also means that `VectorList` can be used with types that cannot be moved.
 * - A `VectorList` can not be indexed efficiently. So while the container is ordered, one can not
 *   efficiently get the value at a specific index.
 * - Iterating over a `VectorList` is a little bit slower, because it may have to iterate over
 *   multiple arrays. That is likely negligible in most cases.
 *
 * `VectorList` should be used instead of `Vector` when the following two statements are true:
 * - The elements do not have to be in a contiguous array.
 * - The elements do not have to be accessed with an index.
 */

#include "BLI_allocator.hh"
#include "BLI_memory_utils.hh"

namespace blender {

template<typename T> struct VectorListChunk {
  /** Start of this chunk. */
  T *begin;
  /** End of this chunk. */
  T *capacity_end;
};
template<typename T,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
         typename Allocator = GuardedAllocator>
class VectorList {
  T *current_end_;
  T *current_capacity_end_;
  VectorListChunk *current_chunk_;
};

}  // namespace blender
