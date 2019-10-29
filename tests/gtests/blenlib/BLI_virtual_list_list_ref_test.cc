#include "testing/testing.h"
#include "BLI_virtual_list_list_ref.h"
#include <vector>
#include <array>

using namespace BLI;

TEST(virtual_list_list_ref, DefaultConstruct)
{
  VirtualListListRef<int> list;
  EXPECT_EQ(list.size(), 0);
}

TEST(virtual_list_list_ref, FromSingleArray)
{
  std::array<int, 3> values = {3, 4, 5};
  VirtualListListRef<int> list = VirtualListListRef<int>::FromSingleArray(values, 6);
  EXPECT_EQ(list.size(), 6);

  EXPECT_EQ(list[0].size(), 3);
  EXPECT_EQ(list[1].size(), 3);
  EXPECT_EQ(list[2].size(), 3);
  EXPECT_EQ(list[3].size(), 3);
  EXPECT_EQ(list[4].size(), 3);
  EXPECT_EQ(list[5].size(), 3);

  EXPECT_EQ(list[2][0], 3);
  EXPECT_EQ(list[2][1], 4);
  EXPECT_EQ(list[2][2], 5);
}

TEST(virtual_list_list_ref, FromListOfStartPointers)
{
  std::array<int, 3> values1 = {1, 2, 3};
  std::array<int, 2> values2 = {4, 5};
  std::array<int, 4> values3 = {6, 7, 8, 9};

  std::array<const int *, 3> starts = {values1.data(), values2.data(), values3.data()};
  std::array<uint, 3> sizes = {values1.size(), values2.size(), values3.size()};

  VirtualListListRef<int> list = VirtualListListRef<int>::FromListOfStartPointers(starts, sizes);

  EXPECT_EQ(list.size(), 3);

  EXPECT_EQ(list[0].size(), 3);
  EXPECT_EQ(list[1].size(), 2);
  EXPECT_EQ(list[2].size(), 4);

  EXPECT_EQ(list[0][0], 1);
  EXPECT_EQ(list[0][1], 2);
  EXPECT_EQ(list[0][2], 3);

  EXPECT_EQ(list[1][0], 4);
  EXPECT_EQ(list[1][1], 5);

  EXPECT_EQ(list[2][0], 6);
  EXPECT_EQ(list[2][1], 7);
  EXPECT_EQ(list[2][2], 8);
  EXPECT_EQ(list[2][3], 9);
}
