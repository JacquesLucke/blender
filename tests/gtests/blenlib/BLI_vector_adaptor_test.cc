#include "testing/testing.h"
#include "BLI_vector_adaptor.hpp"
#include <vector>

using IntVectorAdaptor = BLI::VectorAdaptor<int>;

TEST(vector_adaptor, DefaultConstructor)
{
  IntVectorAdaptor vec;
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 0);
}

TEST(vector_adaptor, PointerConstructor)
{
  int *array = new int[3];
  IntVectorAdaptor vec(array, 3);
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 3);
  delete[] array;
}

TEST(vector_adaptor, ArrayConstructor)
{
  int array[5];
  IntVectorAdaptor vec(array);
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 5);
}

TEST(vector_adaptor, AppendOnce)
{
  int array[5];
  IntVectorAdaptor vec(array);
  vec.append(42);
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0], 42);
}

TEST(vector_adaptor, AppendFull)
{
  int array[5];
  IntVectorAdaptor vec(array);
  vec.append(3);
  vec.append(4);
  vec.append(5);
  vec.append(6);
  vec.append(7);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 4);
  EXPECT_EQ(vec[2], 5);
  EXPECT_EQ(vec[3], 6);
  EXPECT_EQ(vec[4], 7);
}

TEST(vector_adaptor, Iterate)
{
  int array[4];
  IntVectorAdaptor vec(array);
  vec.append(10);
  vec.append(11);
  vec.append(12);

  std::vector<int> std_vector;
  for (int value : vec) {
    std_vector.push_back(value);
  }

  EXPECT_EQ(std_vector.size(), 3);
  EXPECT_EQ(std_vector[0], 10);
  EXPECT_EQ(std_vector[1], 11);
  EXPECT_EQ(std_vector[2], 12);
}

TEST(vector_adaptor, Extend)
{
  int array[6];
  IntVectorAdaptor vec(array);
  vec.extend({1, 3});
  vec.extend({2, 5});
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 2);
  EXPECT_EQ(vec[3], 5);
}

TEST(vector_adaptor, AppendNTimes)
{
  int array[6];
  IntVectorAdaptor vec(array);
  vec.append_n_times(10, 2);
  vec.append_n_times(5, 3);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_EQ(vec[0], 10);
  EXPECT_EQ(vec[1], 10);
  EXPECT_EQ(vec[2], 5);
  EXPECT_EQ(vec[3], 5);
  EXPECT_EQ(vec[4], 5);
}
