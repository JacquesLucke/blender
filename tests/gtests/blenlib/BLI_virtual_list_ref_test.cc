#include "testing/testing.h"
#include "BLI_virtual_list_ref.h"
#include <vector>
#include <array>

using namespace BLI;

TEST(virtual_list_ref, DefaultConstruct)
{
  VirtualListRef<int> list;
  EXPECT_EQ(list.size(), 0);
}

TEST(virtual_list_ref, FromSingle)
{
  int value = 5;
  auto list = VirtualListRef<int>::FromSingle(&value, 3);
  EXPECT_EQ(list.size(), 3);
  EXPECT_EQ(list[0], 5);
  EXPECT_EQ(list[1], 5);
  EXPECT_EQ(list[2], 5);
}

TEST(virtual_list_ref, FromFullArray)
{
  std::vector<int> values = {5, 6, 7, 8};
  auto list = VirtualListRef<int>::FromFullArray(values);
  EXPECT_EQ(list.size(), 4);
  EXPECT_EQ(list[0], 5);
  EXPECT_EQ(list[1], 6);
  EXPECT_EQ(list[2], 7);
  EXPECT_EQ(list[3], 8);
}

TEST(virtual_list_ref, FromFullPointerArray)
{
  int a1 = 3;
  int a2 = 6;
  int a3 = 2;
  std::array<const int *, 5> pointers = {&a1, &a3, &a1, &a2, &a2};

  auto list = VirtualListRef<int>::FromFullPointerArray(pointers);
  EXPECT_EQ(list.size(), 5);
  EXPECT_EQ(list[0], 3);
  EXPECT_EQ(list[1], 2);
  EXPECT_EQ(list[2], 3);
  EXPECT_EQ(list[3], 6);
  EXPECT_EQ(list[4], 6);
}

TEST(virtual_list_ref, FromRepeatedArray)
{
  std::vector<int> values = {3, 4};
  auto list = VirtualListRef<int>::FromRepeatedArray(values, 5);
  EXPECT_EQ(list.size(), 5);
  EXPECT_EQ(list[0], 3);
  EXPECT_EQ(list[1], 4);
  EXPECT_EQ(list[2], 3);
  EXPECT_EQ(list[3], 4);
  EXPECT_EQ(list[4], 3);
}
