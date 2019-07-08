#include "testing/testing.h"
#include "BLI_array_allocator.hpp"

using BLI::ArrayAllocator;

TEST(fixed_array_allocator, AllocateSameSize)
{
  ArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(4);
  void *ptr2 = allocator.allocate(4);
  EXPECT_NE(ptr1, ptr2);
}

TEST(fixed_array_allocator, AllocateDifferentSizes)
{
  ArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(3);
  void *ptr2 = allocator.allocate(4);
  EXPECT_NE(ptr1, ptr2);
}

TEST(fixed_array_allocator, AllocateSamePointerTwice)
{
  ArrayAllocator allocator(42);
  void *ptr1 = allocator.allocate(10);
  allocator.deallocate(ptr1, 10);
  void *ptr2 = allocator.allocate(10);
  EXPECT_EQ(ptr1, ptr2);
}
