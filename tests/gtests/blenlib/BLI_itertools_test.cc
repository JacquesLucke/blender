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

TEST(itertools, ZipEqual2)
{
  Vector<int> vec1 = {6, 10, 50};
  Vector<std::string> vec2 = {"hello", "world", "test"};

  Vector<int> vec1_new;
  Vector<std::string> vec2_new;
  for (auto &&[a, b] : zip_equal(vec1, vec2)) {
    vec1_new.append(a);
    vec2_new.append(b);
  }

  EXPECT_EQ(vec1_new.size(), 3);
  EXPECT_EQ(vec2_new.size(), 3);
  EXPECT_EQ(vec1_new[0], 6);
  EXPECT_EQ(vec1_new[1], 10);
  EXPECT_EQ(vec1_new[2], 50);
  EXPECT_EQ(vec2_new[0], "hello");
  EXPECT_EQ(vec2_new[1], "world");
  EXPECT_EQ(vec2_new[2], "test");
}

TEST(itertools, EnumerateZipEqual2)
{
  Vector<int> vec1 = {6, 7, 8};
  Vector<int> vec2 = {3, 4, 5};

  auto range = enumerate(zip_equal(vec1, vec2), 10);

  auto current = range.begin();
  EXPECT_NE(current, range.end());
  {
    auto item = *current;
    EXPECT_EQ(item.index, 10);
    EXPECT_EQ(std::get<0>(item.value), 6);
    EXPECT_EQ(std::get<1>(item.value), 3);
  }
  ++current;
  EXPECT_NE(current, range.end());
  {
    auto item = *current;
    EXPECT_EQ(item.index, 11);
    EXPECT_EQ(std::get<0>(item.value), 7);
    EXPECT_EQ(std::get<1>(item.value), 4);
  }
  ++current;
  EXPECT_NE(current, range.end());
  {
    auto item = *current;
    EXPECT_EQ(item.index, 12);
    EXPECT_EQ(std::get<0>(item.value), 8);
    EXPECT_EQ(std::get<1>(item.value), 5);
  }
  ++current;
  EXPECT_FALSE(current != range.end());
}

TEST(itertools, ZipEqual4)
{
  Vector<int> vec1 = {4, 5, 6};
  Vector<std::string> vec2 = {"hello", "world", "test"};
  Vector<std::unique_ptr<int>> vec3;
  vec3.append(std::make_unique<int>(10));
  vec3.append(std::make_unique<int>(11));
  vec3.append(std::make_unique<int>(12));
  Vector<int> vec4 = {20, 21, 22};

  for (auto item : zip_equal(vec1, vec2, vec3, vec4)) {
    std::cout << std::get<0>(item) << ", " << std::get<1>(item) << ", " << *std::get<2>(item)
              << ", " << std::get<3>(item) << "\n";
  }
}

}  // namespace blender
