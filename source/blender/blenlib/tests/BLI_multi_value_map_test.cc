/* Apache License, Version 2.0 */

#include "BLI_multi_value_map.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(multi_value_map, Test)
{
  MultiValueMap<int, int> map;
  map.add(2, 4);
  map.add(2, 5);
  map.add(3, 6);

  EXPECT_EQ(map.lookup(2).size(), 2);
  EXPECT_EQ(map.lookup(2)[0], 4);
  EXPECT_EQ(map.lookup(2)[1], 5);

  EXPECT_EQ(map.lookup(3).size(), 1);
  EXPECT_EQ(map.lookup(3)[0], 6);

  EXPECT_EQ(map.lookup(4).size(), 0);
}

}  // namespace blender::tests
