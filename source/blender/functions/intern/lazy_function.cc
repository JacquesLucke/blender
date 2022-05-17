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

}  // namespace blender::fn
