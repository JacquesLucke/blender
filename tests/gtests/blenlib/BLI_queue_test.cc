#include "BLI_queue.hh"
#include "BLI_strict_flags.h"
#include "testing/testing.h"

using namespace BLI;

TEST(queue, DefaultConstructor)
{
  Queue<int> queue;
  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.is_empty());
}

TEST(queue, EnqueueAndDequeueMany)
{
  Queue<int> queue;
  EXPECT_TRUE(queue.is_empty());
  for (int i = 1; i <= 100; i++) {
    queue.enqueue(i);
    EXPECT_EQ(queue.size(), i);
    EXPECT_FALSE(queue.is_empty());
  }
  for (int i = 1; i <= 100; i++) {
    EXPECT_FALSE(queue.is_empty());
    EXPECT_EQ(queue.dequeue(), i);
    EXPECT_EQ(queue.size(), 100 - i);
  }
  EXPECT_TRUE(queue.is_empty());
  for (int i = 0; i < 500; i++) {
    queue.enqueue(i);
    EXPECT_EQ(queue.size(), i + 1);
  }
  for (int i = 0; i < 200; i++) {
    EXPECT_EQ(queue.dequeue(), i);
    EXPECT_EQ(queue.size(), 500 - i - 1);
  }
  for (int i = 500; i < 5000; i++) {
    queue.enqueue(i);
    EXPECT_EQ(queue.size(), i - 200 + 1);
  }
  for (int i = 200; i < 5000; i++) {
    EXPECT_EQ(queue.dequeue(), i);
    EXPECT_EQ(queue.size(), 5000 - i - 1);
  }
}
