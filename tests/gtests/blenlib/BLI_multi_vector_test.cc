#include "testing/testing.h"
#include "BLI_multi_vector.hpp"

using namespace BLI;

using IntMultiVector = MultiVector<int>;

TEST(multi_vector, DefaultConstructor)
{
  IntMultiVector vec;
  EXPECT_EQ(vec.size(), 0);
}

TEST(multi_vector, Append)
{
  IntMultiVector vec;
  vec.append({4, 5, 6});
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0].size(), 3);
}
