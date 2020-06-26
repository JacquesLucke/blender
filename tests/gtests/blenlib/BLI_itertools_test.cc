#include "BLI_itertools.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender {

TEST(itertools, EnumerateConstVector)
{
  Vector<std::unique_ptr<int>> vec;
  vec.append(std::make_unique<int>(3));
  vec.append(std::make_unique<int>(4));
  vec.append(std::make_unique<int>(5));
  const Vector<std::unique_ptr<int>> &const_vec = vec;

  Vector<uint> indices;
  Vector<int> values;
  for (auto item : enumerate(const_vec)) {
    indices.append(item.index);
    values.append(*item.value);
  }

  EXPECT_EQ(indices.size(), 3);
  EXPECT_EQ(values.size(), 3);
  EXPECT_EQ(indices[0], 0);
  EXPECT_EQ(indices[1], 1);
  EXPECT_EQ(indices[2], 2);
  EXPECT_EQ(values[0], 3);
  EXPECT_EQ(values[1], 4);
  EXPECT_EQ(values[2], 5);
}

TEST(itertools, EnumerateMutableVector)
{
  Vector<std::unique_ptr<int>> vec;
  vec.append(std::make_unique<int>(3));
  vec.append(std::make_unique<int>(4));
  vec.append(std::make_unique<int>(5));

  for (auto item : enumerate<int>(vec, 10)) {
    *item.value += item.index;
  }

  EXPECT_EQ(*vec[0], 13);
  EXPECT_EQ(*vec[1], 15);
  EXPECT_EQ(*vec[2], 17);
}

TEST(itertools, EnumerateRValueVector)
{
  for (auto item : enumerate(Vector<int>({6, 7, 8}))) {
    EXPECT_EQ(item.index + 6, item.value);
  }
}

TEST(itertools, EnumerateMultipleTimes)
{
  Vector<int> vec = {6, 7, 8};
  for (auto item : enumerate(enumerate(enumerate(vec)))) {
    EXPECT_EQ(item.index, item.value.index);
    EXPECT_EQ(item.index, item.value.value.index);
    EXPECT_EQ(item.index + 6, item.value.value.value);
    item.value.value.value += 10;
  }

  EXPECT_EQ(vec[0], 16);
  EXPECT_EQ(vec[1], 17);
  EXPECT_EQ(vec[2], 18);
}

}  // namespace blender
