/* Apache License, Version 2.0 */

#include "BLI_strict_flags.h"
#include "BLI_virtual_array.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(virtual_array, ForSpan)
{
  std::array<int, 5> data = {3, 4, 5, 6, 7};
  VArrayForSpan<int> varray{data};
  EXPECT_EQ(varray.size(), 5);
  EXPECT_EQ(varray.get(0), 3);
  EXPECT_EQ(varray.get(4), 7);
  EXPECT_TRUE(varray.is_span());
  EXPECT_FALSE(varray.is_single());
  EXPECT_EQ(varray.get_span().data(), data.data());
}

TEST(virtual_mutable_array, ForMutableSpan)
{
  std::array<int, 5> data = {1, 2, 3, 4, 5};
  VMutableArrayForMutableSpan<int> varray{data};
  EXPECT_EQ(varray.size(), 5);
  EXPECT_EQ(varray.get(0), 1);
  EXPECT_EQ(varray.get(4), 5);
  EXPECT_TRUE(varray.is_span());
  EXPECT_FALSE(varray.is_single());
  EXPECT_EQ(varray.get_span().data(), data.data());
  varray.set(3, 10);
  EXPECT_EQ(data[3], 10);
}

TEST(virtual_array, ForSingle)
{
  VArrayForSingle<int> varray{10, 4};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray.get(0), 10);
  EXPECT_EQ(varray.get(3), 10);
  EXPECT_FALSE(varray.is_span());
  EXPECT_TRUE(varray.is_single());
}

}  // namespace blender::tests
