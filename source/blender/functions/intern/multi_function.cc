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

#include "FN_multi_function.hh"

#include "BLI_task.hh"
#include "BLI_threads.h"

namespace blender::fn {

using ExecutionHints = MultiFunction::ExecutionHints;

ExecutionHints MultiFunction::execution_hints() const
{
  return this->get_execution_hints();
}

ExecutionHints MultiFunction::get_execution_hints() const
{
  return ExecutionHints{};
}

enum class IndexMode {
  Original,
  Moved,
  Compressed,
};

struct ExecutionStrategy {
  IndexMode index_mode = IndexMode::Original;
  int64_t grain_size = 1000;
};

static ExecutionStrategy make_execution_strategy(const MultiFunction &fn, IndexMask mask)
{
  BLI_assert(!mask.is_empty());

  const ExecutionHints hints = fn.execution_hints();

  ExecutionStrategy strategy;
  strategy.grain_size = hints.min_grain_size;

  if (hints.uniform_execution_time) {
    const int thread_count = BLI_system_thread_count();
    /* Avoid using a small grain size even if it is not necessary. */
    const int64_t thread_based_grain_size = mask.size() / thread_count / 4;
    strategy.grain_size = std::max(strategy.grain_size, thread_based_grain_size);
  }

  if (!hints.allocates_array) {
    strategy.index_mode = IndexMode::Original;
  }
  else {
    const int64_t first_index = mask[0];
    const float first_gap_ratio = (float)first_index / (float)mask.min_array_size();
    if (mask.is_range()) {
      if (first_index < 100 || first_gap_ratio < 0.1f) {
        strategy.index_mode = IndexMode::Original;
      }
      else {
        strategy.index_mode = IndexMode::Moved;
      }
    }
    else {
      const int64_t index_spread = mask.last() - first_index;
      const int64_t tot_indices = mask.size();
      const float mask_density = (float)tot_indices / index_spread;
      if (index_spread > 100 && mask_density <= 0.2f) {
        strategy.index_mode = IndexMode::Compressed;
      }
      else if (first_gap_ratio < 0.1f) {
        strategy.index_mode = IndexMode::Original;
      }
      else {
        strategy.index_mode = IndexMode::Moved;
      }
    }
  }
  return strategy;
}

static bool supports_threading_by_slicing_params(const MultiFunction &fn)
{
  for (const int i : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(i);
    if (ELEM(param_type.interface_type(),
             MFParamType::InterfaceType::Mutable,
             MFParamType::InterfaceType::Output)) {
      if (param_type.data_type().is_vector()) {
        return false;
      }
    }
  }
  return true;
}

static void call_with_original_indices(const MultiFunction &fn,
                                       IndexMask orig_mask,
                                       MFParams orig_params,
                                       MFContext orig_context,
                                       const int64_t grain_size)
{
  if (orig_mask.size() <= grain_size) {
    fn.call(orig_mask, orig_params, orig_context);
    return;
  }
  const bool supports_threading = supports_threading_by_slicing_params(fn);
  if (!supports_threading) {
    fn.call(orig_mask, orig_params, orig_context);
    return;
  }

  threading::parallel_for(orig_mask.index_range(), grain_size, [&](const IndexRange sub_range) {
    const IndexMask sub_mask = orig_mask.slice(sub_range);
    fn.call(sub_mask, orig_params, orig_context);
  });
}

static void call_with_moved_indices(const MultiFunction &fn,
                                    IndexMask orig_mask,
                                    MFParams orig_params,
                                    MFContext orig_context,
                                    const int64_t grain_size)
{
  const bool supports_threading = supports_threading_by_slicing_params(fn);
  if (!supports_threading) {
    /* Split using grain size. */
    fn.call(orig_mask, orig_params, orig_context);
    return;
  }
  threading::parallel_for(orig_mask.index_range(), grain_size, [&](const IndexRange sub_range) {
    if (orig_mask[0] == 0 && sub_range[0] == 0) {
      const IndexMask sub_mask = orig_mask.slice(sub_range);
      fn.call(sub_mask, orig_params, orig_context);
      return;
    }
    Vector<int64_t> sub_mask_indices;
    const IndexMask sub_mask = orig_mask.slice_and_offset(sub_range, sub_mask_indices);
    const int64_t input_slice_start = orig_mask[sub_range.first()];
    const int64_t input_slice_size = orig_mask[sub_range.last()] - input_slice_start + 1;
    const IndexRange input_slice_range{input_slice_start, input_slice_size};

    MFParamsBuilder sub_params{fn, sub_mask.min_array_size()};

    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      switch (param_type.category()) {
        case MFParamType::SingleInput: {
          const GVArray &varray = orig_params.readonly_single_input(param_index);
          sub_params.add_readonly_single_input(varray.slice(input_slice_range));
          break;
        }
        case MFParamType::SingleMutable: {
          const GMutableSpan span = orig_params.single_mutable(param_index);
          const GMutableSpan sliced_span = span.slice(input_slice_range);
          sub_params.add_single_mutable(sliced_span);
          break;
        }
        case MFParamType::SingleOutput: {
          const GMutableSpan span = orig_params.uninitialized_single_output_if_required(
              param_index);
          if (span.is_empty()) {
            sub_params.add_ignored_single_output();
          }
          else {
            const GMutableSpan sliced_span = span.slice(input_slice_range);
            sub_params.add_uninitialized_single_output(sliced_span);
          }
          break;
        }
        case MFParamType::VectorInput:
        case MFParamType::VectorMutable:
        case MFParamType::VectorOutput: {
          BLI_assert_unreachable();
          break;
        }
      }
    }

    fn.call(sub_mask, sub_params, orig_context);
  });
}

void MultiFunction::call_auto(IndexMask mask, MFParams params, MFContext context) const
{
  if (mask.is_empty()) {
    return;
  }
  const ExecutionStrategy strategy = make_execution_strategy(*this, mask);
  switch (strategy.index_mode) {
    case IndexMode::Original: {
      call_with_original_indices(*this, mask, params, context, strategy.grain_size);
      break;
    }
    case IndexMode::Moved: {
      call_with_moved_indices(*this, mask, params, context, strategy.grain_size);
      break;
    }
    case IndexMode::Compressed: {
      /* TODO: Implement compressed mode. */
      call_with_moved_indices(*this, mask, params, context, strategy.grain_size);
      break;
    }
  }
}

}  // namespace blender::fn
