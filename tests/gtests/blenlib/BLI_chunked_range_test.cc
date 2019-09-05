#include "testing/testing.h"
#include "BLI_index_range.hpp"
#include "BLI_chunked_range.hpp"
#include "BLI_vector.hpp"

using BLI::ChunkedIndexRange;
using BLI::IndexRange;

TEST(chunked_range, ChunksExact)
{
  IndexRange range = IndexRange(10, 40);
  ChunkedIndexRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 4);

  EXPECT_EQ(chunked_range.chunk_range(0), IndexRange(10, 10));
  EXPECT_EQ(chunked_range.chunk_range(1), IndexRange(20, 10));
  EXPECT_EQ(chunked_range.chunk_range(2), IndexRange(30, 10));
  EXPECT_EQ(chunked_range.chunk_range(3), IndexRange(40, 10));
}

TEST(chunked_range, ChunksMore)
{
  IndexRange range = IndexRange(25, 15);
  ChunkedIndexRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 2);

  EXPECT_EQ(chunked_range.chunk_range(0), IndexRange(25, 10));
  EXPECT_EQ(chunked_range.chunk_range(1), IndexRange(35, 5));
}

TEST(chunked_range, ChunksZero)
{
  IndexRange range = IndexRange(20, 0);
  ChunkedIndexRange chunked_range(range, 10);
  EXPECT_EQ(chunked_range.chunks(), 0);
}
