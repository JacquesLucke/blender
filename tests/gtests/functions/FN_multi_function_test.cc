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

#include "FN_cpp_types.hh"
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

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<int> a = params.readonly_single_input<int>(0, "A");
    VSpan<int> b = params.readonly_single_input<int>(1, "B");
    MutableSpan<int> result = params.uninitialized_single_output<int>(2, "Result");

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
  params.add_uninitialized_single_output(output.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2}, params, context);

  EXPECT_EQ(output[0], 14);
  EXPECT_EQ(output[1], -1);
  EXPECT_EQ(output[2], 36);
}

class AddPrefixFunction : public MultiFunction {
 public:
  AddPrefixFunction()
  {
    MFSignatureBuilder builder = this->get_builder("Add Prefix");
    builder.single_input<std::string>("Prefix");
    builder.single_mutable<std::string>("Strings");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<std::string> prefixes = params.readonly_single_input<std::string>(0, "Prefix");
    MutableSpan<std::string> strings = params.single_mutable<std::string>(1, "Strings");

    for (uint i : mask) {
      strings[i] = prefixes[i] + strings[i];
    }
  }
};

TEST(multi_function, AddPrefixFunction)
{
  AddPrefixFunction fn;

  Array<std::string> strings = {
      "Hello",
      "World",
      "This is a test",
      "Another much longer string to trigger an allocation",
  };

  std::string prefix = "AB";

  MFParamsBuilder params(fn, strings.size());
  params.add_readonly_single_input(&prefix);
  params.add_single_mutable(strings.as_mutable_span());

  MFContextBuilder context;

  fn.call({0, 2, 3}, params, context);

  EXPECT_EQ(strings[0], "ABHello");
  EXPECT_EQ(strings[1], "World");
  EXPECT_EQ(strings[2], "ABThis is a test");
  EXPECT_EQ(strings[3], "ABAnother much longer string to trigger an allocation");
}

}  // namespace fn
}  // namespace blender
