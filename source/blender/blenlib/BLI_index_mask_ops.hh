/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This is separate from `BLI_index_mask.hh` because it includes headers just `IndexMask` shouldn't
 * depend on.
 */

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

namespace blender::index_mask_ops {

namespace detail {
IndexMask find_indices_based_on_predicate__merge(
    IndexMask indices_to_check,
    threading::EnumerableThreadSpecific<Vector<Vector<int64_t>>> &sub_masks,
    Vector<int64_t> &r_indices);
}  // namespace detail

/**
 * Evaluate the #predicate for all indices in #indices_to_check and return a mask that contains all
 * indices where the predicate was true.
 *
 * #r_indices indices is only used if necessary.
 */
template<typename Predicate>
inline IndexMask find_indices_based_on_predicate(const IndexMask indices_to_check,
                                                 const int64_t parallel_grain_size,
                                                 Vector<int64_t> &r_indices,
                                                 const Predicate &predicate)
{
  /* Evaluate predicate in parallel. Since the size of the final mask is not known yet, many
   * smaller vectors have to be filled with all indices where the predicate is true. Those smaller
   * vectors are joined afterwards. */
  threading::EnumerableThreadSpecific<Vector<Vector<int64_t>>> sub_masks;
  threading::parallel_for(
      indices_to_check.index_range(), parallel_grain_size, [&](const IndexRange range) {
        const IndexMask sub_mask = indices_to_check.slice(range);
        Vector<int64_t> masked_indices;
        for (const int64_t i : sub_mask) {
          if (predicate(i)) {
            masked_indices.append(i);
          }
        }
        if (!masked_indices.is_empty()) {
          sub_masks.local().append(std::move(masked_indices));
        }
      });

  /* This part doesn't have to be in the header. */
  return detail::find_indices_based_on_predicate__merge(indices_to_check, sub_masks, r_indices);
}

void compress_ranges(Span<IndexRange> ranges, MutableSpan<IndexRange> r_compressed_ranges);

template<typename T>
void get_element_ranges(const Span<IndexRange> ranges,
                        const Span<T> offsets,
                        MutableSpan<IndexRange> r_element_ranges)
{
  threading::parallel_for(ranges.index_range(), 1024, [&](const IndexRange thread_range) {
    for (const int64_t i : thread_range) {
      const IndexRange range = ranges[i];
      const int64_t start = static_cast<int64_t>(offsets[range.start()]);
      const int64_t end = static_cast<int64_t>(offsets[range.one_after_last()]);
      const int64_t size = end - start;
      const IndexRange element_range{start, size};
      r_element_ranges[i] = element_range;
    }
  });
}

}  // namespace blender::index_mask_ops
