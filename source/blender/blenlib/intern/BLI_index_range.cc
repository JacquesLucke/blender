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

#include <atomic>
#include <mutex>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

namespace blender {

static RawVector<RawArray<int64_t, 0>> arrays;
static int64_t current_array_size = 0;
static int64_t *current_array = nullptr;
static std::mutex current_array_mutex;

Span<int64_t> IndexRange::as_span() const
{
  int64_t min_required_size = start_ + size_;

  if (min_required_size <= current_array_size) {
    return Span<int64_t>(current_array + start_, size_);
  }

  std::lock_guard<std::mutex> lock(current_array_mutex);

  if (min_required_size <= current_array_size) {
    return Span<int64_t>(current_array + start_, size_);
  }

  int64_t new_size = std::max<int64_t>(1000, power_of_2_max_u(min_required_size));
  RawArray<int64_t, 0> new_array(new_size);
  for (int64_t i = 0; i < new_size; i++) {
    new_array[i] = i;
  }
  arrays.append(std::move(new_array));

  current_array = arrays.last().data();
  std::atomic_thread_fence(std::memory_order_seq_cst);
  current_array_size = new_size;

  return Span<int64_t>(current_array + start_, size_);
}

AlignedIndexRanges split_index_range_by_alignment(const IndexRange range, const int64_t alignment)
{
  BLI_assert(is_power_of_2_i(alignment));
  const int64_t mask = alignment - 1;

  AlignedIndexRanges aligned_ranges;

  const int64_t start_chunk = range.start() & ~mask;
  const int64_t end_chunk = range.one_after_last() & ~mask;
  if (start_chunk == end_chunk) {
    aligned_ranges.prefix = range;
  }
  else {
    int64_t prefix_size = 0;
    int64_t suffix_size = 0;
    if (range.start() != start_chunk) {
      prefix_size = alignment - (range.start() & mask);
    }
    if (range.one_after_last() != end_chunk) {
      suffix_size = range.one_after_last() - end_chunk;
    }
    aligned_ranges.prefix = IndexRange(range.start(), prefix_size);
    aligned_ranges.suffix = IndexRange(end_chunk, suffix_size);
    aligned_ranges.aligned = IndexRange(aligned_ranges.prefix.one_after_last(),
                                        range.size() - prefix_size - suffix_size);
  }

  return aligned_ranges;
}

}  // namespace blender
