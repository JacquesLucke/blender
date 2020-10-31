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

#pragma once

#include "FN_multi_function.hh"

namespace blender::fn {

template<typename T> T mf_eval_1_SO(const MultiFunction &fn)
{
  MFParamsBuilder params(fn, 1);
  MFContextBuilder context;
  Array<T> output_values{1, NoInitialization{}};
  params.add_uninitialized_single_output(output_values.as_mutable_span());
  fn.call({0}, params, context);
  return output_values[0];
}

template<typename InT, typename OutT>
OutT mf_eval_1_SI_SO(const MultiFunction &fn, InT input_value)
{
  MFParamsBuilder params(fn, 1);
  MFContextBuilder context;
  Array<OutT> output_values{1, NoInitialization{}};
  params.add_readonly_single_input(&input_value);
  params.add_uninitialized_single_output(output_values.as_mutable_span());
  fn.call({0}, params, context);
  return output_values[0];
}

}  // namespace blender::fn
