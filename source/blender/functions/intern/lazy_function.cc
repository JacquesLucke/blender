/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fn
 */

#include "BLI_array.hh"

#include "FN_lazy_function.hh"

namespace blender::fn {

std::string LazyFunction::name() const
{
  return static_name_;
}

std::string LazyFunction::input_name(int index) const
{
  return inputs_[index].static_name;
}

std::string LazyFunction::output_name(int index) const
{
  return outputs_[index].static_name;
}

void *LazyFunction::init_storage(LinearAllocator<> &UNUSED(allocator)) const
{
  return nullptr;
}

void LazyFunction::destruct_storage(void *storage) const
{
  BLI_assert(storage == nullptr);
  UNUSED_VARS_NDEBUG(storage);
}

bool LazyFunction::valid_params_for_execution(const LazyFunctionParams &params) const
{
  bool all_required_inputs_available = true;
  for (const int i : inputs_.index_range()) {
    const LazyFunctionInput &fn_input = inputs_[i];
    if (fn_input.usage == ValueUsage::Used) {
      if (params.try_get_input_data_ptr(i) == nullptr) {
        all_required_inputs_available = false;
        break;
      }
    }
  }
  bool any_remaining_output_left = false;
  for (const int i : outputs_.index_range()) {
    if (params.get_output_usage(i) != ValueUsage::Unused) {
      if (!params.output_was_set(i)) {
        any_remaining_output_left = true;
        break;
      }
    }
  }
  return all_required_inputs_available && any_remaining_output_left;
}

}  // namespace blender::fn
