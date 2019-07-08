#include "testing/testing.h"
#include "BLI_fixed_array_allocator.hpp"

using BLI::FixedArrayAllocator;

TEST(fixed_array_allocator, AllocateSameSize)
{
  FixedArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(4);
  void *ptr2 = allocator.allocate(4);
  EXPECT_NE(ptr1, ptr2);
}

TEST(fixed_array_allocator, AllocateDifferentSizes)
{
  FixedArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(3);
  void *ptr2 = allocator.allocate(4);
  EXPECT_NE(ptr1, ptr2);
}

TEST(fixed_array_allocator, AllocateSamePointerTwice)
{
  FixedArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(10);
  allocator.deallocate(ptr1, 10);
  void *ptr2 = allocator.allocate(10);
  EXPECT_EQ(ptr1, ptr2);
}
