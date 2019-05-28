#include "testing/testing.h"
#include "BLI_string_ref.hpp"

using BLI::StringRef;
using BLI::StringRefNull;

TEST(string_ref_null, DefaultConstructor)
{
  StringRefNull ref;
  EXPECT_EQ(ref.size(), 0);
  EXPECT_EQ(ref[0], '\0');
}

TEST(string_ref_null, CStringConstructor)
{
  const char *str = "Hello";
  StringRefNull ref(str);
  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref.data(), str);
}

TEST(string_ref_null, CStringLengthConstructor)
{
  const char *str = "Hello";
  StringRefNull ref(str, 5);
  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref.data(), str);
}

TEST(string_ref, DefaultConstructor)
{
  StringRef ref;
  EXPECT_EQ(ref.size(), 0);
}

TEST(string_ref, CStringConstructor)
{
  const char *str = "Test";
  StringRef ref(str);
  EXPECT_EQ(ref.size(), 4);
  EXPECT_EQ(ref.data(), str);
}

TEST(string_ref, PointerWithLengthConstructor)
{
  const char *str = "Test";
  StringRef ref(str, 2);
  EXPECT_EQ(ref.size(), 2);
  EXPECT_EQ(ref.data(), str);
}

TEST(string_ref, StdStringConstructor)
{
  std::string str = "Test";
  StringRef ref(str);
  EXPECT_EQ(ref.size(), 4);
  EXPECT_EQ(ref.data(), str.data());
}

TEST(string_ref, SubscriptOperator)
{
  StringRef ref("hello");
  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref[0], 'h');
  EXPECT_EQ(ref[1], 'e');
  EXPECT_EQ(ref[2], 'l');
  EXPECT_EQ(ref[3], 'l');
  EXPECT_EQ(ref[4], 'o');
}

TEST(string_ref, ToStdString)
{
  StringRef ref("test");
  std::string str = ref.to_std_string();
  EXPECT_EQ(str.size(), 4);
  EXPECT_EQ(str, "test");
}

TEST(string_ref, Print)
{
  StringRef ref("test");
  std::stringstream ss;
  ss << ref;
  ss << ref;
  std::string str = ss.str();
  EXPECT_EQ(str.size(), 8);
  EXPECT_EQ(str, "testtest");
}

TEST(string_ref, Add)
{
  StringRef a("qwe");
  StringRef b("asd");
  std::string result = a + b;
  EXPECT_EQ(result, "qweasd");
}

TEST(string_ref, AddCharPtr1)
{
  StringRef ref("test");
  std::string result = ref + "qwe";
  EXPECT_EQ(result, "testqwe");
}

TEST(string_ref, AddCharPtr2)
{
  StringRef ref("test");
  std::string result = "qwe" + ref;
  EXPECT_EQ(result, "qwetest");
}

TEST(string_ref, AddString1)
{
  StringRef ref("test");
  std::string result = ref + std::string("asd");
  EXPECT_EQ(result, "testasd");
}

TEST(string_ref, AddString2)
{
  StringRef ref("test");
  std::string result = std::string("asd") + ref;
  EXPECT_EQ(result, "asdtest");
}
