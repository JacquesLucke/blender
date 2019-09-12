#include "testing/testing.h"
#include "BLI_chained_strings.h"

using BLI::ChainedStringRef;
using BLI::ChainedStringsBuilder;

TEST(chained_strings, BuildEmpty)
{
  ChainedStringsBuilder builder;
  char *str = builder.build();
  EXPECT_NE(str, nullptr);
  MEM_freeN(str);
}

TEST(chained_strings, BuildSingleString)
{
  ChainedStringsBuilder builder;
  auto ref = builder.add("Hello");

  char *str = builder.build();

  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref.to_string_ref(str), "Hello");

  MEM_freeN(str);
}

TEST(chained_strings, BuildMultiple)
{
  ChainedStringsBuilder builder;
  auto ref1 = builder.add("Why");
  auto ref2 = builder.add("What");
  auto ref3 = builder.add("Where");

  char *str = builder.build();

  EXPECT_EQ(ref1.size(), 3);
  EXPECT_EQ(ref2.size(), 4);
  EXPECT_EQ(ref3.size(), 5);

  EXPECT_EQ(ref1.to_string_ref(str), "Why");
  EXPECT_EQ(ref2.to_string_ref(str), "What");
  EXPECT_EQ(ref3.to_string_ref(str), "Where");

  MEM_freeN(str);
}
