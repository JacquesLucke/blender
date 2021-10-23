/* Apache License, Version 2.0 */

#include "BLI_any.hh"
#include "BLI_map.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(any, DefaultConstructor)
{
  Any a;
  EXPECT_TRUE(a.is_empty());
}

TEST(any, AssignInt)
{
  Any a = 5;
  EXPECT_FALSE(a.is_empty());
  EXPECT_TRUE(a.is<int>());
  EXPECT_FALSE(a.is<float>());
  const int &value = a.get<int>();
  EXPECT_EQ(value, 5);
  a = 10;
  EXPECT_EQ(value, 10);

  Any b = a;
  EXPECT_FALSE(b.is_empty());
  EXPECT_EQ(b.get<int>(), 10);

  Any c = std::move(a);
  EXPECT_FALSE(c.is_empty());
  EXPECT_EQ(c.get<int>(), 10);

  EXPECT_EQ(a.get<int>(), 10); /* NOLINT: bugprone-use-after-move */

  a.reset();
  EXPECT_TRUE(a.is_empty());
}

TEST(any, AssignMap)
{
  Any a = Map<int, int>();
  EXPECT_FALSE(a.is_empty());
  EXPECT_TRUE((a.is<Map<int, int>>()));
  EXPECT_FALSE((a.is<Map<int, float>>()));
  Map<int, int> &map = a.get<Map<int, int>>();
  map.add(4, 2);
  EXPECT_EQ((a.get<Map<int, int>>().lookup(4)), 2);

  Any b = a;
  EXPECT_FALSE(b.is_empty());
  EXPECT_EQ((b.get<Map<int, int>>().lookup(4)), 2);

  Any c = std::move(a);
  c = c;
  EXPECT_FALSE(b.is_empty());
  EXPECT_EQ((c.get<Map<int, int>>().lookup(4)), 2);

  EXPECT_TRUE((a.get<Map<int, int>>().is_empty())); /* NOLINT: bugprone-use-after-move */
}

TEST(any, AssignAny)
{
  Any a = 5;
  Any b = std::string("hello");
  Any c;

  Any z;
  EXPECT_TRUE(z.is_empty());

  z = a;
  EXPECT_FALSE(z.is_empty());
  EXPECT_EQ(z.get<int>(), 5);

  z = b;
  EXPECT_EQ(z.get<std::string>(), "hello");

  z = c;
  EXPECT_TRUE(z.is_empty());

  z = Any(std::in_place_type<Any<>>, a);
  EXPECT_FALSE(z.is<int>());
  EXPECT_TRUE(z.is<Any<>>());
  EXPECT_EQ(z.get<Any<>>().get<int>(), 5);
}

}  // namespace blender::tests
