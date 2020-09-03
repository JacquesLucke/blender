/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_string_matching.h"

namespace blender::string_matching::text {

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
}

}  // namespace blender::string_matching::text
