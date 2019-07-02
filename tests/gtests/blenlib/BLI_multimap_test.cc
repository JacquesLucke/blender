#include "testing/testing.h"
#include "BLI_multimap.hpp"

using namespace BLI;

using IntMap = MultiMap<int, int>;

TEST(multimap, DefaultConstructor)
{
  IntMap map;
  EXPECT_EQ(map.size(), 0);
}

TEST(multimap, AddNewSingle)
{
  IntMap map;
  map.add_new(2, 5);
  EXPECT_EQ(map.size(), 1);
  EXPECT_TRUE(map.contains(2));
  EXPECT_FALSE(map.contains(5));
  EXPECT_EQ(map.lookup(2)[0], 5);
}

TEST(multimap, AddMultipleforSameKey)
{
  IntMap map;
  map.add(3, 5);
  map.add(3, 1);
  map.add(3, 7);
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.lookup(3).size(), 3);
  EXPECT_EQ(map.lookup(3)[0], 5);
  EXPECT_EQ(map.lookup(3)[1], 1);
  EXPECT_EQ(map.lookup(3)[2], 7);
}

TEST(multimap, AddMany)
{
  IntMap map;
  for (uint i = 0; i < 100; i++) {
    int key = i % 10;
    map.add(key, i);
  }

  EXPECT_EQ(map.size(), 10);
  EXPECT_TRUE(map.contains(3));
  EXPECT_FALSE(map.contains(11));
  EXPECT_EQ(map.lookup(2)[4], 42);
  EXPECT_EQ(map.lookup(6)[1], 16);
  EXPECT_EQ(map.lookup(7).size(), 10);
}

TEST(multimap, ValuesForKey)
{
  IntMap map;
  map.add(3, 5);
  map.add(3, 7);
  map.add(3, 8);
  map.add(4, 2);
  map.add(4, 3);
  EXPECT_EQ(map.values_for_key(3), 3);
  EXPECT_EQ(map.values_for_key(4), 2);
}
