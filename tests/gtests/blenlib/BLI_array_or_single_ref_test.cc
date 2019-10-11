#include "testing/testing.h"
#include "BLI_array_or_single_ref.h"

using namespace BLI;

TEST(array_ref, DefaultConstruct)
{
  ArrayOrSingleRef<int> ref;
  EXPECT_EQ(ref.size(), 0);
}
