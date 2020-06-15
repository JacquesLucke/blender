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

#include "testing/testing.h"

#include "FN_multi_function.hh"

namespace blender {
namespace fn {

class AddFunction : public MultiFunction {
 public:
  AddFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Add");
    builder.single_input<int>("A");
    builder.single_input<int>("B");
    builder.single_output<int>("Result");
  }

  void call(IndexMask mask, MFParams params, MFContext context) const override
  {
    VSpan<int> a = params.readonly_single_input(0, "A").typed<int>();
    VSpan<int> b = params.readonly_single_input(1, "B").typed<int>();
    MutableSpan<int> result = params.uninitialized_single_output(2, "Result").typed<int>();

    for (uint i : mask) {
      result[i] = a[i] + b[i];
    }
  }
};

TEST(multi_function, AddFunction)
{
  AddFunction fn;

  Array<int> input1 = {4, 5, 6};
  Array<int> input2 = {10, 20, 30};
  Array<int> output(3, -1);

  MFParamsBuilder params(fn, 3);
  params.add_readonly_single_input(input1.as_span());
  params.add_readonly_single_input(input2.as_span());
  params.add_single_output(output.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2}, params, context);

  EXPECT_EQ(output[0], 14);
  EXPECT_EQ(output[1], -1);
  EXPECT_EQ(output[2], 36);
}

}  // namespace fn
}  // namespace blender
