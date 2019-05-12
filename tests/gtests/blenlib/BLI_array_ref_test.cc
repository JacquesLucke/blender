#include "testing/testing.h"
#include "BLI_array_ref.hpp"

using IntVector = BLI::SmallVector<int>;
using IntArrayRef = BLI::ArrayRef<int>;

TEST(array_ref, FromSmallVector)
{
  IntVector a = {1, 2, 3};
  IntArrayRef a_ref = a;
  EXPECT_EQ(a_ref.size(), 3);
  EXPECT_EQ(a_ref[0], 1);
  EXPECT_EQ(a_ref[1], 2);
  EXPECT_EQ(a_ref[2], 3);
}

TEST(array_ref, IsReferencing)
{
  int array[] = {3, 5, 8};
  IntArrayRef ref(array, ARRAY_SIZE(array));
  EXPECT_EQ(ref.size(), 3);
  EXPECT_EQ(ref[1], 5);
  array[1] = 10;
  EXPECT_EQ(ref[1], 10);
}

TEST(array_ref, DropBack)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).drop_back(2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 4);
  EXPECT_EQ(slice[1], 5);
}

TEST(array_ref, DropBackAll)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).drop_back(a.size());
  EXPECT_EQ(slice.size(), 0);
}

TEST(array_ref, DropFront)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).drop_front(1);
  EXPECT_EQ(slice.size(), 3);
  EXPECT_EQ(slice[0], 5);
  EXPECT_EQ(slice[1], 6);
  EXPECT_EQ(slice[2], 7);
}

TEST(array_ref, DropFrontAll)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).drop_front(a.size());
  EXPECT_EQ(slice.size(), 0);
}

TEST(array_ref, Slice)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).slice(1, 2);
  EXPECT_EQ(slice.size(), 2);
  EXPECT_EQ(slice[0], 5);
  EXPECT_EQ(slice[1], 6);
}

TEST(array_ref, SliceEmpty)
{
  IntVector a = {4, 5, 6, 7};
  auto slice = IntArrayRef(a).slice(2, 0);
  EXPECT_EQ(slice.size(), 0);
}

TEST(array_ref, Contains)
{
  IntVector a = {4, 5, 6, 7};
  IntArrayRef a_ref = a;
  EXPECT_TRUE(a_ref.contains(4));
  EXPECT_TRUE(a_ref.contains(5));
  EXPECT_TRUE(a_ref.contains(6));
  EXPECT_TRUE(a_ref.contains(7));
  EXPECT_FALSE(a_ref.contains(3));
  EXPECT_FALSE(a_ref.contains(8));
}

TEST(array_ref, Count)
{
  IntVector a = {2, 3, 4, 3, 3, 2, 2, 2, 2};
  IntArrayRef a_ref = a;
  EXPECT_EQ(a_ref.count(1), 0);
  EXPECT_EQ(a_ref.count(2), 5);
  EXPECT_EQ(a_ref.count(3), 3);
  EXPECT_EQ(a_ref.count(4), 1);
  EXPECT_EQ(a_ref.count(5), 0);
}

TEST(array_ref, ToSmallVector)
{
  IntVector a = {1, 2, 3, 4};
  IntArrayRef a_ref = a;
  IntVector b = a_ref.to_small_vector();
  IntVector::all_equal(a, b);
}

static void test_ref_from_initializer_list(IntArrayRef ref)
{
  EXPECT_EQ(ref.size(), 4);
  EXPECT_EQ(ref[0], 3);
  EXPECT_EQ(ref[1], 6);
  EXPECT_EQ(ref[2], 8);
  EXPECT_EQ(ref[3], 9);
}

TEST(array_ref, FromInitializerList)
{
  test_ref_from_initializer_list({3, 6, 8, 9});
}

TEST(array_ref, FromSingleValue)
{
  int a = 4;
  IntArrayRef a_ref(a);
  EXPECT_EQ(a_ref.size(), 1);
  EXPECT_EQ(a_ref[0], 4);
  a = 10;
  EXPECT_EQ(a_ref[0], 10);
}
