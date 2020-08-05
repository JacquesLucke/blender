/* Apache License, Version 2.0 */

#include "BLI_trie_set.hh"
#include "testing/testing.h"

namespace blender::tests {

TEST(trie_set, Test)
{
  TrieSet trie;
  EXPECT_EQ(trie.size(), 0);

  EXPECT_TRUE(trie.add("qwer"));
  EXPECT_TRUE(trie.add("qwar"));
  EXPECT_FALSE(trie.add("qwer"));
  EXPECT_TRUE(trie.add("qw"));

  EXPECT_TRUE(trie.has_prefix_of("qwerty"));
  EXPECT_FALSE(trie.has_prefix_of("qzxc"));
  EXPECT_TRUE(trie.has_prefix_of("qwa"));
  EXPECT_TRUE(trie.has_prefix_of("qw"));
  EXPECT_FALSE(trie.has_prefix_of("q"));
  EXPECT_FALSE(trie.has_prefix_of(""));
}

}  // namespace blender::tests
