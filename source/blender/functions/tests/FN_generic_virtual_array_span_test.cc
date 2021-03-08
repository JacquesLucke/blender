/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_generic_virtual_array_span.hh"

#include "BLI_array.hh"

namespace blender::fn::tests {

TEST(generic_virtual_array_span, TypeConstructor)
{
  GVArraySpanForStartsAndSizes span{CPPType::get<int32_t>(), {}, {}};
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());

  VArraySpanForGVArraySpan<int> converted{span};
  EXPECT_EQ(converted.size(), 0);
}

TEST(generic_virtual_array_span, GSpanConstructor)
{
  std::array<std::string, 3> values = {"hello", "world", "test"};
  GVArraySpanForSingleGSpan span{GSpan(CPPType::get<std::string>(), values.data(), 3), 5};
  EXPECT_EQ(span.size(), 5);
  EXPECT_FALSE(span.is_empty());
  std::string a, b, c;
  span.get_array_element(0, 0, &a);
  span.get_array_element(1, 2, &b);
  span.get_array_element(4, 1, &c);
  EXPECT_EQ(a, "hello");
  EXPECT_EQ(b, "test");
  EXPECT_EQ(c, "world");
  EXPECT_EQ(span.get_array_size(0), 3);
  EXPECT_EQ(span.get_array_size(2), 3);

  VArraySpanForGVArraySpan<std::string> converted{span};
  EXPECT_EQ(converted.size(), 5);
  EXPECT_EQ(converted.get_array_element(0, 0), "hello");
  EXPECT_EQ(converted.get_array_element(1, 1), "world");
  EXPECT_EQ(converted.get_array_element(4, 2), "test");
  EXPECT_EQ(converted.get_array_size(0), 3);
  EXPECT_EQ(converted.get_array_size(2), 3);
}

TEST(generic_virtual_array_span, IsSingleArray1)
{
  Array<int> values = {5, 6, 7};
  GVArraySpanForSingleGSpan span{GSpan(values.as_span()), 4};
  EXPECT_TRUE(span.is_single_array());

  VArraySpanForGVArraySpan<int> converted{span};
  EXPECT_TRUE(converted.is_single_array());
}

}  // namespace blender::fn::tests
