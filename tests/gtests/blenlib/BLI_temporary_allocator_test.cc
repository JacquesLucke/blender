#include "testing/testing.h"
#include "BLI_temporary_allocator.hpp"

using namespace BLI;

TEST(temporary_vector, Construct)
{
  TemporaryVector<int> vec(5);
  EXPECT_EQ(vec->size(), 0);
  EXPECT_EQ(vec->capacity(), 5);
}

TEST(temporary_vector, Append)
{
  TemporaryVector<int> vec(4);
  vec->append(3);
  vec->append(6);
  EXPECT_EQ(vec->size(), 2);
  EXPECT_EQ(vec->capacity(), 4);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 6);
}
