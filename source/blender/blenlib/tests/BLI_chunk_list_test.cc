/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_chunk_list.hh"
#include "BLI_timeit.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(chunk_list, Test)
{
  const int64_t amount = 3;
  for ([[maybe_unused]] const int64_t iter : IndexRange(5)) {
    {
      ChunkList<int, 2> list;
      {
        SCOPED_TIMER("chunk list: create");
        for (const int64_t i : IndexRange(amount)) {
          list.append(i);
        }
      }
      int sum = 0;
      {
        SCOPED_TIMER("chunk list: sum");
        // list.foreach_elem([&](const int v) { sum += v; });
        for (const int v : list) {
          sum += v;
        }
      }
      std::cout << "Sum: " << sum << "\n";
    }
    {
      Vector<int, 2> vec;
      {

        SCOPED_TIMER("vector: create");
        for (const int64_t i : IndexRange(amount)) {
          vec.append(i);
        }
      }
      int sum = 0;
      {
        SCOPED_TIMER("vector: sum");
        for (const int v : vec) {
          sum += v;
        }
      }
      std::cout << "Sum: " << sum << "\n";
    }
  }
}

TEST(chunk_list, Stack)
{
  ChunkList<int64_t> list;
  const int64_t amount = 1e5;
  for (const int64_t i : IndexRange(amount)) {
    list.append(i);
  }
  EXPECT_EQ(list.size(), amount);
  for (const int64_t i : IndexRange(amount)) {
    const int popped_value = list.pop_last();
    EXPECT_EQ(popped_value, amount - i - 1);
  }
  EXPECT_EQ(list.size(), 0);
}

}  // namespace blender::tests
