/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_chunk_list.hh"
#include "BLI_timeit.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(chunk_list, Test)
{
  const int64_t amount = 1e9;
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
      Vector<int> vec;
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

}  // namespace blender::tests
