#include "BLI_stack.hh"
#include "testing/testing.h"

using BLI::Stack;

TEST(stack, DefaultConstructor)
{
  Stack<int> stack;
  EXPECT_EQ(stack.size(), 0);
  EXPECT_TRUE(stack.is_empty());
}

TEST(stack, ArrayRefConstructor)
{
  std::array<int, 3> array = {4, 7, 2};
  Stack<int> stack(array);
  EXPECT_EQ(stack.size(), 3);
  EXPECT_EQ(stack.pop(), 2);
  EXPECT_EQ(stack.pop(), 7);
  EXPECT_EQ(stack.pop(), 4);
  EXPECT_TRUE(stack.is_empty());
}

TEST(stack, Push)
{
  Stack<int> stack;
  EXPECT_EQ(stack.size(), 0);
  stack.push(3);
  EXPECT_EQ(stack.size(), 1);
  stack.push(5);
  EXPECT_EQ(stack.size(), 2);
}

TEST(stack, PushMultiple)
{
  Stack<int> stack;
  EXPECT_EQ(stack.size(), 0);
  stack.push_multiple({1, 2, 3});
  EXPECT_EQ(stack.size(), 3);
  EXPECT_EQ(stack.pop(), 3);
  EXPECT_EQ(stack.pop(), 2);
  EXPECT_EQ(stack.pop(), 1);
}

TEST(stack, PushPopMany)
{
  Stack<int> stack;
  for (int i = 0; i < 1000; i++) {
    stack.push(i);
    EXPECT_EQ(stack.size(), i + 1);
  }
  for (int i = 999; i > 50; i--) {
    EXPECT_EQ(stack.pop(), i);
    EXPECT_EQ(stack.size(), i);
  }
  for (int i = 51; i < 5000; i++) {
    stack.push(i);
    EXPECT_EQ(stack.size(), i + 1);
  }
  for (int i = 4999; i >= 0; i--) {
    EXPECT_EQ(stack.pop(), i);
    EXPECT_EQ(stack.size(), i);
  }
}

TEST(stack, Pop)
{
  Stack<int> stack;
  stack.push(4);
  stack.push(6);
  EXPECT_EQ(stack.pop(), 6);
  EXPECT_EQ(stack.pop(), 4);
}

TEST(stack, Peek)
{
  Stack<int> stack;
  stack.push(3);
  stack.push(4);
  EXPECT_EQ(stack.peek(), 4);
  EXPECT_EQ(stack.peek(), 4);
  stack.pop();
  EXPECT_EQ(stack.peek(), 3);
}

TEST(stack, UniquePtrValues)
{
  Stack<std::unique_ptr<int>> stack;
  stack.push(std::unique_ptr<int>(new int()));
  stack.push(std::unique_ptr<int>(new int()));
  std::unique_ptr<int> a = stack.pop();
  std::unique_ptr<int> &b = stack.peek();
  UNUSED_VARS(a, b);
}
