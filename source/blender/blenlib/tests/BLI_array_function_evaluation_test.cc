/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array_function_evaluation.hh"
#include <array>

#include "testing/testing.h"

namespace blender::array_function_evaluation::tests {

TEST(array_function_evaluation, Test)
{
  std::array<int, 5> inputs = {1, 2, 3, 4, 5};
  std::array<int, 5> outputs;

  execute_materialized([](const int a, int &b) { b = a + 10; },
                       IndexRange(5),
                       std::make_index_sequence<2>(),
                       ArrayInput<int>(inputs.data()),
                       ArrayOutput<int>(outputs.data()));

  EXPECT_EQ(outputs[0], 11);
  EXPECT_EQ(outputs[1], 12);
  EXPECT_EQ(outputs[2], 13);
  EXPECT_EQ(outputs[3], 14);
  EXPECT_EQ(outputs[4], 15);
}

}  // namespace blender::array_function_evaluation::tests
