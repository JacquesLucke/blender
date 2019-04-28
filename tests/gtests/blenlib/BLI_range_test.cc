#include "testing/testing.h"
#include "BLI_range.hpp"
#include "BLI_small_vector.hpp"

using IntRange = BLI::Range<int>;
using IntVector = BLI::SmallVector<int>;

TEST(range, DefaultConstructor)
{
  IntRange range;
  EXPECT_EQ(range.size(), 0);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }
  EXPECT_EQ(vector.size(), 0);
}

TEST(range, SingleElementRange)
{
  IntRange range(4, 5);
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(*range.begin(), 4);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 1);
  EXPECT_EQ(vector[0], 4);
}

TEST(range, MultipleElementRange)
{
  IntRange range(6, 10);
  EXPECT_EQ(range.size(), 4);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 4);
  for (uint i = 0; i < 4; i++) {
    EXPECT_EQ(vector[i], i + 6);
  }
}

TEST(range, SubscriptOperator)
{
  IntRange range(5, 10);
  EXPECT_EQ(range[0], 5);
  EXPECT_EQ(range[1], 6);
  EXPECT_EQ(range[2], 7);
}
