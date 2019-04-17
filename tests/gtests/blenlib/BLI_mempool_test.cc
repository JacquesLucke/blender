#include "testing/testing.h"
#include "BLI_mempool.hpp"

using namespace BLI;

TEST(mempool, Test1)
{
  MemPool mempool(sizeof(int));
  int *a = new (mempool.allocate()) int;
  *a = 3;
  EXPECT_EQ(*a, 3);
  mempool.deallocate(a);
}

TEST(mempool, Test2)
{
  MemPool mempool(sizeof(int));
  for (uint i = 0; i < 100000; i++) {
    mempool.allocate();
  }
}