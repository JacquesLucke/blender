/* Apache License, Version 2.0 */

#include "BLI_array.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
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

TEST(virtual_array, ForSingle)
{
  VArrayForSingle<int> varray{10, 4};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray.get(0), 10);
  EXPECT_EQ(varray.get(3), 10);
  EXPECT_FALSE(varray.is_span());
  EXPECT_TRUE(varray.is_single());
}

TEST(virtual_array, ForArray)
{
  Array<int> array = {1, 2, 3, 5, 8};
  {
    VArrayForArrayContainer varray{array};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArrayForArrayContainer varray{std::move(array)};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArrayForArrayContainer varray{array}; /* NOLINT: bugprone-use-after-move */
    EXPECT_TRUE(varray.is_empty());
  }
}

TEST(virtual_array, ForVector)
{
  Vector<int> vector = {9, 8, 7, 6};
  VArrayForArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 9);
  EXPECT_EQ(varray[3], 6);
}

TEST(virtual_array, ForStdVector)
{
  std::vector<int> vector = {5, 6, 7, 8};
  VArrayForArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 6);
}

TEST(virtual_array, ForStdArray)
{
  std::array<int, 4> array = {2, 3, 4, 5};
  VArrayForArrayContainer varray{array};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 2);
  EXPECT_EQ(varray[1], 3);
}

TEST(virtual_array, ForVectorSet)
{
  VectorSet<int> vector_set = {5, 3, 7, 3, 3, 5, 1};
  VArrayForArrayContainer varray{std::move(vector_set)};
  EXPECT_TRUE(vector_set.is_empty()); /* NOLINT: bugprone-use-after-move. */
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 3);
  EXPECT_EQ(varray[2], 7);
  EXPECT_EQ(varray[3], 1);
}

}  // namespace blender::tests
