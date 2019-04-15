#include "testing/testing.h"
#include "BLI_small_set.hpp"

using IntSet = BLI::SmallSet<int>;

TEST(small_set, Defaultconstructor)
{
	IntSet set;
	EXPECT_EQ(set.size(), 0);
}

TEST(small_set, ContainsNotExistant)
{
	IntSet set;
	EXPECT_FALSE(set.contains(3));
}

TEST(small_set, ContainsExistant)
{
	IntSet set;
	EXPECT_FALSE(set.contains(5));
	set.add(5);
	EXPECT_TRUE(set.contains(5));
}

TEST(small_set, AddMany)
{
	IntSet set;
	for (int i = 0; i < 100; i++) {
		set.add(i);
	}

	for (int i = 50; i < 100; i++) {
		EXPECT_TRUE(set.contains(i));
	}
	for (int i = 100; i < 150; i++) {
		EXPECT_FALSE(set.contains(i));
	}
}

TEST(small_set, InitializerListConstructor)
{
	IntSet set = {4, 5, 6};
	EXPECT_EQ(set.size(), 3);
	EXPECT_TRUE(set.contains(4));
	EXPECT_TRUE(set.contains(5));
	EXPECT_TRUE(set.contains(6));
	EXPECT_FALSE(set.contains(2));
	EXPECT_FALSE(set.contains(3));
}

TEST(small_set, CopyConstructor)
{
	IntSet set = {3};
	EXPECT_TRUE(set.contains(3));
	EXPECT_FALSE(set.contains(4));

	IntSet set2 = set;
	set2.add(4);
	EXPECT_TRUE(set2.contains(3));
	EXPECT_TRUE(set2.contains(4));

	EXPECT_FALSE(set.contains(4));
}

TEST(small_set, MoveConstructor)
{
	IntSet set = {1, 2, 3};
	EXPECT_EQ(set.size(), 3);
	IntSet set2 = std::move(set);
	EXPECT_EQ(set.size(), 0);
	EXPECT_EQ(set2.size(), 3);
}

TEST(small_set, Pop)
{
	IntSet set = {4};
	EXPECT_EQ(set.size(), 1);
	EXPECT_EQ(set.pop(), 4);
	EXPECT_EQ(set.size(), 0);
}

TEST(small_set, Remove)
{
	IntSet set = {3, 4, 5};
	EXPECT_TRUE(set.contains(3));
	EXPECT_TRUE(set.contains(4));
	EXPECT_TRUE(set.contains(5));
	set.remove(4);
	EXPECT_TRUE(set.contains(3));
	EXPECT_FALSE(set.contains(4));
	EXPECT_TRUE(set.contains(5));
	set.remove(3);
	EXPECT_FALSE(set.contains(3));
	EXPECT_FALSE(set.contains(4));
	EXPECT_TRUE(set.contains(5));
	set.remove(5);
	EXPECT_FALSE(set.contains(3));
	EXPECT_FALSE(set.contains(4));
	EXPECT_FALSE(set.contains(5));
}
