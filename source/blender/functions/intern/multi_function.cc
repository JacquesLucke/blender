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

#include "BLI_task.hh"
#include "FN_multi_function.hh"

namespace blender::fn {

static constexpr int64_t default_grain_size = 1000;
static constexpr int64_t default_memory_slice_size = 10000;

bool MultiFunction::is_primitive() const
{
  return true;
}

int64_t MultiFunction::grain_size() const
{
  return default_grain_size;
}

static void prepare_sliced_parameters(const MultiFunction &fn,
                                      const MFParams params,
                                      const IndexRange slice,
                                      MFParamsBuilder &r_sliced_params)
{
  ResourceScope &scope = r_sliced_params.resource_scope();

  /* All parameters are sliced so that the wrapped multi-function does not have to take
   * care of the index offset. */
  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    switch (param_type.category()) {
      case MFParamType::SingleInput: {
        const GVArray &varray = params.readonly_single_input(param_index);
        const GVArray &sliced_varray = scope.construct<GVArray_Slice>(varray, slice);
        r_sliced_params.add_readonly_single_input(sliced_varray);
        break;
      }
      case MFParamType::SingleMutable: {
        const GMutableSpan span = params.single_mutable(param_index);
        const GMutableSpan sliced_span = span.slice(slice.start(), slice.size());
        r_sliced_params.add_single_mutable(sliced_span);
        break;
      }
      case MFParamType::SingleOutput: {
        const GMutableSpan span = params.uninitialized_single_output_if_required(param_index);
        if (span.is_empty()) {
          r_sliced_params.add_ignored_single_output();
        }
        else {
          const GMutableSpan sliced_span = span.slice(slice.start(), slice.size());
          r_sliced_params.add_uninitialized_single_output(sliced_span);
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
}

static void process_offset_mask_slice(const MultiFunction &fn,
                                      const IndexRange mask_slice,
                                      IndexMask full_mask,
                                      MFParams params,
                                      MFContext context)
{
  Vector<int64_t> sub_mask_indices;
  const IndexMask sub_mask = full_mask.slice_and_offset(mask_slice, sub_mask_indices);
  if (sub_mask.is_empty()) {
    return;
  }
  const int64_t input_slice_start = full_mask[mask_slice.first()];
  const int64_t input_slice_size = full_mask[mask_slice.last()] - input_slice_start + 1;
  const IndexRange input_slice_range{input_slice_start, input_slice_size};

  MFParamsBuilder sub_params{fn, sub_mask.min_array_size()};
  prepare_sliced_parameters(fn, params, input_slice_range, sub_params);
  fn.call(sub_mask, sub_params, context);
}

static void process_mask_slice(const MultiFunction &fn,
                               const IndexRange mask_slice,
                               IndexMask full_mask,
                               MFParams params,
                               MFContext context)
{
  const int64_t array_size = full_mask.min_array_size();
  MFParamsBuilder sub_params{fn, array_size};
  prepare_sliced_parameters(fn, params, IndexRange(array_size), sub_params);
  fn.call(full_mask.slice(mask_slice), sub_params, context);
}

void MultiFunction::call_auto(IndexMask mask, MFParams params, MFContext context) const
{
  const bool supports_threading = !signature_ref_->has_vector_param;
  if (this->is_primitive()) {
    const int64_t grain_size = this->grain_size();
    if (supports_threading && mask.size() > grain_size) {
      threading::parallel_for(mask.index_range(), grain_size, [&](const IndexRange mask_slice) {
        process_mask_slice(*this, mask_slice, mask, params, context);
      });
      return;
    }
  }
  else if (mask.size() > default_memory_slice_size) {
    if (supports_threading) {
      threading::parallel_for(
          mask.index_range(), default_memory_slice_size, [&](const IndexRange mask_slice) {
            process_offset_mask_slice(*this, mask_slice, mask, params, context);
          });
    }
    else {
      for (int i = 0; i < mask.size(); i += default_memory_slice_size) {
        const IndexRange mask_slice{i, std::min(default_memory_slice_size, mask.size() - i)};
        process_offset_mask_slice(*this, mask_slice, mask, params, context);
      };
    }
    return;
  }

  this->call(mask, params, context);
}

class DummyMultiFunction : public MultiFunction {
 public:
  DummyMultiFunction()
  {
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Dummy"};
    return signature.build();
  }

  void call(IndexMask UNUSED(mask),
            MFParams UNUSED(params),
            MFContext UNUSED(context)) const override
  {
  }
};

static DummyMultiFunction dummy_multi_function_;
const MultiFunction &dummy_multi_function = dummy_multi_function_;

}  // namespace blender::fn
