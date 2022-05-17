/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_lazy_function.hh"

namespace blender::fn {

std::string LazyFunction::name() const
{
  return "Unnamed Function";
}

std::string LazyFunction::input_name(int index) const
{
  return "Input " + std::to_string(index);
}

std::string LazyFunction::output_name(int index) const
{
  return "Output " + std::to_string(index);
}

}  // namespace blender::fn
