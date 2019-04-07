#include "testing/testing.h"
#include "BLI_array_lookup.hpp"

using IntArrayLookup = BLI::ArrayLookup<int>;

TEST(array_lookup, Contains)
{
    int array[] = {10, 4, 6};
    IntArrayLookup lookup;
    EXPECT_FALSE(lookup.contains(array, 10));
    EXPECT_FALSE(lookup.contains(array, 4));
    EXPECT_FALSE(lookup.contains(array, 6));

    lookup.add_new(array, 0);
    lookup.add_new(array, 1);
    lookup.add_new(array, 2);

    EXPECT_TRUE(lookup.contains(array, 10));
    EXPECT_TRUE(lookup.contains(array, 4));
    EXPECT_TRUE(lookup.contains(array, 6));

    EXPECT_FALSE(lookup.contains(array, 5));
}
