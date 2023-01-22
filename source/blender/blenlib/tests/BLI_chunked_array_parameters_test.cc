/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_chunked_array_parameters.hh"
#include <array>

#include "testing/testing.h"

namespace blender::chunked_array_parameters::tests {

TEST(chunked_array_parameters, Test)
{
  std::array<int, 5> inputs = {1, 2, 3, 4, 5};
  std::array<int, 5> outputs = {-1, -1, -1, -1, -1};

  execute_chunked(
      IndexMask({0, 2, 3}),
      [](int size, const int *a, int *b) {
        for (const int i : IndexRange(size)) {
          b[i] = a[i] + 10;
        }
      },
      ArrayInput<int>(inputs.data()),
      ArrayOutput<int>(outputs.data()));

  EXPECT_EQ(outputs[0], 11);
  EXPECT_EQ(outputs[1], -1);
  EXPECT_EQ(outputs[2], 13);
  EXPECT_EQ(outputs[3], 14);
  EXPECT_EQ(outputs[4], -1);
}

}  // namespace blender::chunked_array_parameters::tests
