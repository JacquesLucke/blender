#include "testing/testing.h"
#include "BLI_small_map.hpp"

using IntFloatMap = BLI::SmallMap<int, float>;

TEST(small_map, DefaultConstructor)
{
	IntFloatMap map;
	EXPECT_EQ(map.size(), 0);
}

TEST(small_map, AddIncreasesSize)
{
	IntFloatMap map;
	EXPECT_EQ(map.size(), 0);
	map.add(2, 5.0f);
	EXPECT_EQ(map.size(), 1);
	map.add(6, 2.0f);
	EXPECT_EQ(map.size(), 2);
}

TEST(small_map, Contains)
{
	IntFloatMap map;
	EXPECT_FALSE(map.contains(4));
	map.add(5, 6.0f);
	EXPECT_FALSE(map.contains(4));
	map.add(4, 2.0f);
	EXPECT_TRUE(map.contains(4));
}

TEST(small_map, LookupExisting)
{
	IntFloatMap map;
	map.add(2, 6.0f);
	map.add(4, 1.0f);
	EXPECT_EQ(map.lookup(2), 6.0f);
	EXPECT_EQ(map.lookup(4), 1.0f);
}

TEST(small_map, LookupNotExisting)
{
	IntFloatMap map;
	map.add(2, 4.0f);
	map.add(1, 1.0f);
	EXPECT_EQ(map.lookup_ptr(0), nullptr);
	EXPECT_EQ(map.lookup_ptr(5), nullptr);
}

TEST(small_map, AddMany)
{
	IntFloatMap map;
	for (int i = 0; i < 100; i++) {
		map.add(i, i);
	}
}