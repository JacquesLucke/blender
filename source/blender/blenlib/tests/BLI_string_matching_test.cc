/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_string_matching.h"
#include "BLI_vector.hh"

namespace blender::string_matching::tests {

TEST(string_matching, levenshtein_distance)
{
  EXPECT_EQ(levenshtein_distance("test", "test"), 0);
  EXPECT_EQ(levenshtein_distance("hello", "ell"), 2);
  EXPECT_EQ(levenshtein_distance("hello", "hel"), 2);
  EXPECT_EQ(levenshtein_distance("ell", "hello"), 2);
  EXPECT_EQ(levenshtein_distance("hell", "hello"), 1);
  EXPECT_EQ(levenshtein_distance("hello", "hallo"), 1);
  EXPECT_EQ(levenshtein_distance("test", ""), 4);
  EXPECT_EQ(levenshtein_distance("", "hello"), 5);
  EXPECT_EQ(levenshtein_distance("Test", "test"), 1);
}

TEST(string_matching, initial_characters_distance)
{
  EXPECT_EQ(initial_characters_distance("www", "where why what"), 0);
  EXPECT_EQ(initial_characters_distance("www", "where is what"), 1);
  EXPECT_EQ(initial_characters_distance("www", "where why what test"), 1);
  EXPECT_EQ(initial_characters_distance("asa", "add selected to active collection"), 2);
  EXPECT_EQ(initial_characters_distance("astac", "add selected to active collection"), 0);
}

template<typename Range> static void print_range(const Range &range, StringRef name = "")
{
  Vector<std::string> values;
  int total_length = 0;
  for (const auto &value : range) {
    std::stringstream ss;
    ss << value;
    std::string str = ss.str();
    total_length += str.size();
    values.append(std::move(str));
  }

  if (total_length + values.size() < 70) {
    if (!name.is_empty()) {
      std::cout << name << ": ";
    }
    for (const int i : values.index_range()) {
      std::cout << values[i];
      if (i < values.size() - 1) {
        std::cout << ", ";
      }
    }
    std::cout << '\n';
  }
  else {
    if (!name.is_empty()) {
      std::cout << name << ": \n";
    }
    for (StringRef value : values) {
      std::cout << "  " << value << '\n';
    }
  }
}

TEST(string_matching, filter_and_sort)
{
  Array<StringRef> results = {"all transforms",
                              "all transforms to deltas",
                              "location",
                              "location to deltas",
                              "make instances real",
                              "rotation",
                              "rotation & scale",
                              "rotation to deltas",
                              "scale"};
  filter_and_sort("location to", results);
  print_range(results);
}

}  // namespace blender::string_matching::tests
