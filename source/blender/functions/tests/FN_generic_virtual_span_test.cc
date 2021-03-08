/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_generic_virtual_span.hh"

namespace blender::fn::tests {

TEST(generic_virtual_span, Empty)
{
  GVSpanForGSpan span(CPPType::get<int32_t>());
  EXPECT_EQ(span.size(), 0);
  EXPECT_TRUE(span.is_empty());
  EXPECT_FALSE(span.is_single());
  EXPECT_TRUE(span.is_span());

  VSpanForGVSpan<int> converted{span};
  EXPECT_EQ(converted.size(), 0);
}

TEST(generic_virtual_span, FromSpan)
{
  int values[4] = {3, 4, 5, 6};
  GVSpanForGSpan span{GSpan(CPPType::get<int32_t>(), values, 4)};
  EXPECT_EQ(span.size(), 4);
  EXPECT_FALSE(span.is_empty());
  int value;
  span.get(0, &value);
  EXPECT_EQ(value, values[0]);
  span.get(1, &value);
  EXPECT_EQ(value, values[1]);
  EXPECT_FALSE(span.is_single());
  EXPECT_TRUE(span.is_span());

  int materialized[4] = {0};
  span.materialize_to_uninitialized(materialized);
  EXPECT_EQ(materialized[0], 3);
  EXPECT_EQ(materialized[1], 4);
  EXPECT_EQ(materialized[2], 5);
  EXPECT_EQ(materialized[3], 6);

  VSpanForGVSpan<int> converted{span};
  EXPECT_EQ(converted.size(), 4);
  EXPECT_EQ(converted[0], 3);
  EXPECT_EQ(converted[1], 4);
  EXPECT_EQ(converted[2], 5);
  EXPECT_EQ(converted[3], 6);
}

TEST(generic_virtual_span, FromSingleValue)
{
  int value = 5;
  GVSpanForSingleValue span{CPPType::get<int32_t>(), 3, &value};
  EXPECT_EQ(span.size(), 3);
  EXPECT_FALSE(span.is_empty());
  int a, b, c;
  span.get(0, &a);
  span.get(1, &b);
  span.get(2, &c);
  EXPECT_EQ(a, value);
  EXPECT_EQ(b, value);
  EXPECT_EQ(c, value);
  EXPECT_TRUE(span.is_single());
  int single_value;
  span.get_single(&single_value);
  EXPECT_EQ(single_value, value);
  EXPECT_FALSE(span.is_span());

  int materialized[3] = {0};
  span.materialize_to_uninitialized({1, 2}, materialized);
  EXPECT_EQ(materialized[0], 0);
  EXPECT_EQ(materialized[1], 5);
  EXPECT_EQ(materialized[2], 5);

  VSpanForGVSpan<int> converted{span};
  EXPECT_EQ(converted.size(), 3);
  EXPECT_EQ(converted[0], 5);
  EXPECT_EQ(converted[1], 5);
  EXPECT_EQ(converted[2], 5);
}

}  // namespace blender::fn::tests
