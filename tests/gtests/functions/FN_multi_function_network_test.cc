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
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network.hh"
#include "FN_multi_function_network_evaluation.hh"

namespace blender {
namespace fn {

TEST(multi_function_network, InitialTest)
{
  CustomFunction_SI_SO<int, int> add_10_fn("add 10", [](int value) { return value + 10; });
  CustomFunction_SI_SI_SO<int, int, int> multiply_fn("multiply",
                                                     [](int a, int b) { return a * b; });

  MFNetwork network;

  MFNode &node1 = network.add_function(add_10_fn);
  MFNode &node2 = network.add_function(multiply_fn);
  MFNode &node3 = network.add_dummy("My Dummy", {MFDataType::ForSingle<int>()}, {}, {"Value"}, {});
  MFNode &node4 = network.add_dummy("My Dummy", {}, {MFDataType::ForSingle<int>()}, {}, {"Value"});
  network.add_link(node1.output(0), node2.input(0));
  network.add_link(node1.output(0), node2.input(1));
  network.add_link(node2.output(0), node3.input(0));
  network.add_link(node4.output(0), node1.input(0));

  MFNetworkEvaluator network_fn{{&node4.output(0)}, {&node3.input(0)}};

  Array<int> values = {4, 6, 1, 2, 0};
  Array<int> results(values.size(), 0);

  MFParamsBuilder params(network_fn, values.size());
  params.add_readonly_single_input(values.as_span());
  params.add_uninitialized_single_output(results.as_mutable_span());

  MFContextBuilder context;

  network_fn.call({0, 2, 3, 4}, params, context);

  EXPECT_EQ(results[0], 14 * 14);
  EXPECT_EQ(results[1], 0);
  EXPECT_EQ(results[2], 11 * 11);
  EXPECT_EQ(results[3], 12 * 12);
  EXPECT_EQ(results[4], 10 * 10);
}

}  // namespace fn
}  // namespace blender
