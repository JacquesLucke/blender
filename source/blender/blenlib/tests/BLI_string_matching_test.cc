/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_string_matching.h"
#include "BLI_vector.hh"

namespace blender::string_matching::tests {

TEST(string_matching, damerau_levenshtein_distance)
{
  EXPECT_EQ(damerau_levenshtein_distance("test", "test"), 0);
  EXPECT_EQ(damerau_levenshtein_distance("hello", "ell"), 2);
  EXPECT_EQ(damerau_levenshtein_distance("hello", "hel"), 2);
  EXPECT_EQ(damerau_levenshtein_distance("ell", "hello"), 2);
  EXPECT_EQ(damerau_levenshtein_distance("hell", "hello"), 1);
  EXPECT_EQ(damerau_levenshtein_distance("hello", "hallo"), 1);
  EXPECT_EQ(damerau_levenshtein_distance("test", ""), 4);
  EXPECT_EQ(damerau_levenshtein_distance("", "hello"), 5);
  EXPECT_EQ(damerau_levenshtein_distance("Test", "test"), 1);
  EXPECT_EQ(damerau_levenshtein_distance("ab", "ba"), 1);
  EXPECT_EQ(damerau_levenshtein_distance("what", "waht"), 1);
  EXPECT_EQ(damerau_levenshtein_distance("what", "ahwt"), 2);
}

}  // namespace blender::string_matching::tests
