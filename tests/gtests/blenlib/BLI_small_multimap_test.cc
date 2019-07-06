#include "testing/testing.h"
#include "BLI_small_multimap.hpp"

using namespace BLI;

using IntMultiMap = SmallMultiMap<int, int>;

TEST(multimap, DefaultConstructor)
{
  IntMultiMap map;
  EXPECT_EQ(map.key_amount(), 0);
}

TEST(multimap, AddNewSingle)
{
  IntMultiMap map;
  map.add_new(2, 5);
  EXPECT_EQ(map.key_amount(), 1);
  EXPECT_TRUE(map.contains(2));
  EXPECT_FALSE(map.contains(5));
  EXPECT_EQ(map.lookup(2)[0], 5);
}

TEST(multimap, AddMultipleforSameKey)
{
  IntMultiMap map;
  map.add(3, 5);
  map.add(3, 1);
  map.add(3, 7);
  EXPECT_EQ(map.key_amount(), 1);
  EXPECT_EQ(map.lookup(3).size(), 3);
  EXPECT_EQ(map.lookup(3)[0], 5);
  EXPECT_EQ(map.lookup(3)[1], 1);
  EXPECT_EQ(map.lookup(3)[2], 7);
}

TEST(multimap, AddMany)
{
  IntMultiMap map;
  for (uint i = 0; i < 100; i++) {
    int key = i % 10;
    map.add(key, i);
  }

  EXPECT_EQ(map.key_amount(), 10);
  EXPECT_TRUE(map.contains(3));
  EXPECT_FALSE(map.contains(11));
  EXPECT_EQ(map.lookup(2)[4], 42);
  EXPECT_EQ(map.lookup(6)[1], 16);
  EXPECT_EQ(map.lookup(7).size(), 10);
}

TEST(multimap, AddMultipleNew)
{
  IntMultiMap map;
  map.add_multiple_new(3, {6, 7, 8});
  map.add_multiple_new(2, {1, 2, 5, 7});

  EXPECT_EQ(map.key_amount(), 2);
  EXPECT_TRUE(map.contains(3));
  EXPECT_TRUE(map.contains(2));
  EXPECT_TRUE(map.lookup(2).contains(2));
  EXPECT_FALSE(map.lookup(2).contains(3));
}

TEST(multimap, ValuesForKey)
{
  IntMultiMap map;
  map.add(3, 5);
  map.add(3, 7);
  map.add(3, 8);
  map.add(4, 2);
  map.add(4, 3);
  EXPECT_EQ(map.value_amount(3), 3);
  EXPECT_EQ(map.value_amount(4), 2);
}

TEST(multimap, Keys)
{
  IntMultiMap map;
  map.add(3, 6);
  map.add(3, 3);
  map.add(3, 4);
  map.add(4, 1);
  map.add(2, 1);

  SmallVector<int> values;
  for (auto value : map.keys()) {
    values.append(value);
  }
  EXPECT_EQ(values.size(), 3);
  EXPECT_TRUE(values.contains(3));
  EXPECT_TRUE(values.contains(4));
  EXPECT_TRUE(values.contains(2));
}
