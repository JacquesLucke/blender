#include "testing/testing.h"
#include "BLI_small_set_vector.hpp"

using IntSetVector = BLI::SmallSetVector<int>;

TEST(small_set_vector, DefaultConstructor)
{
	IntSetVector set;
	EXPECT_EQ(set.size(), 0);
}

TEST(small_set_vector, InitializerListConstructor_WithoutDuplicates)
{
	IntSetVector set = {1, 4, 5};
	EXPECT_EQ(set.size(), 3);
	EXPECT_EQ(set[0], 1);
	EXPECT_EQ(set[1], 4);
	EXPECT_EQ(set[2], 5);
}

TEST(small_set_vector, InitializerListConstructor_WithDuplicates)
{
	IntSetVector set = {1, 3, 3, 2, 1, 5};
	EXPECT_EQ(set.size(), 4);
	EXPECT_EQ(set[0], 1);
	EXPECT_EQ(set[1], 3);
	EXPECT_EQ(set[2], 2);
	EXPECT_EQ(set[3], 5);
}

TEST(small_set_vector, AddNewIncreasesSize)
{
	IntSetVector set;
	EXPECT_EQ(set.size(), 0);
	set.add(5);
	EXPECT_EQ(set.size(), 1);
}

TEST(small_set_vector, AddExistingDoesNotIncreaseSize)
{
	IntSetVector set;
	EXPECT_EQ(set.size(), 0);
	set.add(5);
	EXPECT_EQ(set.size(), 1);
	set.add(5);
	EXPECT_EQ(set.size(), 1);
}

TEST(small_set_vector, IndexOfExisting)
{
	IntSetVector set = {3, 6, 4};
	EXPECT_EQ(set.index(6), 1);
	EXPECT_EQ(set.index(3), 0);
	EXPECT_EQ(set.index(4), 2);
}

TEST(small_set_vector, IndexOfNotExisting)
{
	IntSetVector set = {3, 6, 4};
	EXPECT_EQ(set.index(5), -1);
}