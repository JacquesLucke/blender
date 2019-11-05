#include "testing/testing.h"
#include "BLI_monotonic_allocator.h"

using namespace BLI;

static bool is_aligned(void *ptr, uint alignment)
{
  BLI_assert(is_power_of_2_i(alignment));
  return (POINTER_AS_UINT(ptr) & (alignment - 1)) == 0;
}

TEST(monotonic_allocator, AllocationAlignment)
{
  MonotonicAllocator<> allocator;

  EXPECT_TRUE(is_aligned(allocator.allocate(10, 4), 4));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 4), 4));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 4), 4));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 8), 8));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 4), 4));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 16), 16));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 4), 4));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 64), 64));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 64), 64));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 8), 8));
  EXPECT_TRUE(is_aligned(allocator.allocate(10, 128), 128));
}

TEST(monotonic_allocator, PackedAllocation)
{
  MonotonicAllocator<256> allocator;
  allocator.allocate(32, 32);

  uintptr_t ptr1 = (uintptr_t)allocator.allocate(10, 4); /*  0 - 10 */
  uintptr_t ptr2 = (uintptr_t)allocator.allocate(10, 4); /* 12 - 22 */
  uintptr_t ptr3 = (uintptr_t)allocator.allocate(8, 32); /* 32 - 40 */
  uintptr_t ptr4 = (uintptr_t)allocator.allocate(16, 8); /* 40 - 56 */
  uintptr_t ptr5 = (uintptr_t)allocator.allocate(1, 8);  /* 56 - 57 */
  uintptr_t ptr6 = (uintptr_t)allocator.allocate(1, 4);  /* 60 - 61 */
  uintptr_t ptr7 = (uintptr_t)allocator.allocate(1, 1);  /* 61 - 62 */

  EXPECT_EQ(ptr2 - ptr1, 12); /* 12 -  0 = 12 */
  EXPECT_EQ(ptr3 - ptr2, 20); /* 32 - 12 = 20 */
  EXPECT_EQ(ptr4 - ptr3, 8);  /* 40 - 32 =  8 */
  EXPECT_EQ(ptr5 - ptr4, 16); /* 56 - 40 = 16 */
  EXPECT_EQ(ptr6 - ptr5, 4);  /* 60 - 56 =  4 */
  EXPECT_EQ(ptr7 - ptr6, 1);  /* 61 - 60 =  1 */
}
