#include "testing/testing.h"
#include "BLI_range.hpp"
#include "BLI_vector.hpp"

using BLI::ArrayRef;
using IntRange = BLI::Range<int>;
using ChunkedIntRange = BLI::ChunkedRange<int>;
using IntVector = BLI::Vector<int>;

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

TEST(range, Before)
{
  IntRange range = IntRange(5, 10).before(3);
  EXPECT_EQ(range[0], 2);
  EXPECT_EQ(range[1], 3);
  EXPECT_EQ(range[2], 4);
  EXPECT_EQ(range.size(), 3);
}

TEST(range, After)
{
  IntRange range = IntRange(5, 10).after(4);
  EXPECT_EQ(range[0], 10);
  EXPECT_EQ(range[1], 11);
  EXPECT_EQ(range[2], 12);
  EXPECT_EQ(range[3], 13);
  EXPECT_EQ(range.size(), 4);
}

TEST(range, Contains)
{
  IntRange range = IntRange(5, 8);
  EXPECT_TRUE(range.contains(5));
  EXPECT_TRUE(range.contains(6));
  EXPECT_TRUE(range.contains(7));
  EXPECT_FALSE(range.contains(4));
  EXPECT_FALSE(range.contains(8));
}

TEST(range, First)
{
  IntRange range = IntRange(5, 8);
  EXPECT_EQ(range.first(), 5);
}

TEST(range, Last)
{
  IntRange range = IntRange(5, 8);
  EXPECT_EQ(range.last(), 7);
}

TEST(range, OneAfterEnd)
{
  IntRange range = IntRange(5, 8);
  EXPECT_EQ(range.one_after_last(), 8);
}

TEST(range, AsArrayRef)
{
  IntRange range = IntRange(4, 10);
  ArrayRef<int> ref = range.as_array_ref();
  EXPECT_EQ(ref.size(), 6);
  EXPECT_EQ(ref[0], 4);
  EXPECT_EQ(ref[1], 5);
  EXPECT_EQ(ref[2], 6);
  EXPECT_EQ(ref[3], 7);
}

TEST(chunked_range, ChunksExact)
{
  IntRange range = IntRange(10, 50);
  ChunkedIntRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 4);

  EXPECT_EQ(chunked_range.chunk_range(0), IntRange(10, 20));
  EXPECT_EQ(chunked_range.chunk_range(1), IntRange(20, 30));
  EXPECT_EQ(chunked_range.chunk_range(2), IntRange(30, 40));
  EXPECT_EQ(chunked_range.chunk_range(3), IntRange(40, 50));
}

TEST(chunked_range, ChunksMore)
{
  IntRange range = IntRange(25, 40);
  ChunkedIntRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 2);

  EXPECT_EQ(chunked_range.chunk_range(0), IntRange(25, 35));
  EXPECT_EQ(chunked_range.chunk_range(1), IntRange(35, 40));
}

TEST(chunked_range, ChunksZero)
{
  IntRange range = IntRange(20, 20);
  ChunkedIntRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 0);
}
