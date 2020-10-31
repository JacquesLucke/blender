/* Apache License, Version 2.0 */

#include "FN_lang_tokenize.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

TEST(fn_lang_tokenize, IgnoreWhitespace)
{
  StringRef str = "hello world\t a   b\n\n c 5";
  TokenizeResult result = tokenize(str);

  EXPECT_EQ(result.types.size(), 6);
  EXPECT_EQ(result.types[0], TokenType::Identifier);
  EXPECT_EQ(result.types[1], TokenType::Identifier);
  EXPECT_EQ(result.types[2], TokenType::Identifier);
  EXPECT_EQ(result.types[3], TokenType::Identifier);
  EXPECT_EQ(result.types[4], TokenType::Identifier);
  EXPECT_EQ(result.types[5], TokenType::IntLiteral);

  EXPECT_EQ(result.ranges.size(), 6);
  EXPECT_EQ(result.ranges[0].get(str), "hello");
  EXPECT_EQ(result.ranges[1].get(str), "world");
  EXPECT_EQ(result.ranges[2].get(str), "a");
  EXPECT_EQ(result.ranges[3].get(str), "b");
  EXPECT_EQ(result.ranges[4].get(str), "c");
  EXPECT_EQ(result.ranges[5].get(str), "5");
}

TEST(fn_lang_tokenize, TokenizeNumbers)
{
  StringRef str = "1 23 456 4.0 3.1 9. 2.1";
  TokenizeResult result = tokenize(str);

  EXPECT_EQ(result.types.size(), 7);
  EXPECT_EQ(result.types[0], TokenType::IntLiteral);
  EXPECT_EQ(result.types[1], TokenType::IntLiteral);
  EXPECT_EQ(result.types[2], TokenType::IntLiteral);
  EXPECT_EQ(result.types[3], TokenType::FloatLiteral);
  EXPECT_EQ(result.types[4], TokenType::FloatLiteral);
  EXPECT_EQ(result.types[5], TokenType::FloatLiteral);
  EXPECT_EQ(result.types[6], TokenType::FloatLiteral);

  EXPECT_EQ(result.ranges.size(), 7);
  EXPECT_EQ(result.ranges[0].get(str), "1");
  EXPECT_EQ(result.ranges[1].get(str), "23");
  EXPECT_EQ(result.ranges[2].get(str), "456");
  EXPECT_EQ(result.ranges[3].get(str), "4.0");
  EXPECT_EQ(result.ranges[4].get(str), "3.1");
  EXPECT_EQ(result.ranges[5].get(str), "9.");
  EXPECT_EQ(result.ranges[6].get(str), "2.1");
}

TEST(fn_lang_tokenize, Operators)
{
  StringRef str = "+-*/,.()=<>";
  TokenizeResult result = tokenize(str);

  EXPECT_EQ(result.types.size(), 11);
  EXPECT_EQ(result.types[0], TokenType::Plus);
  EXPECT_EQ(result.types[1], TokenType::Minus);
  EXPECT_EQ(result.types[2], TokenType::Asterix);
  EXPECT_EQ(result.types[3], TokenType::ForwardSlash);
  EXPECT_EQ(result.types[4], TokenType::Comma);
  EXPECT_EQ(result.types[5], TokenType::Dot);
  EXPECT_EQ(result.types[6], TokenType::ParenOpen);
  EXPECT_EQ(result.types[7], TokenType::ParenClose);
  EXPECT_EQ(result.types[8], TokenType::Equal);
  EXPECT_EQ(result.types[9], TokenType::IsLess);
  EXPECT_EQ(result.types[10], TokenType::IsGreater);
}

TEST(fn_lang_tokenize, Comparisons)
{
  EXPECT_EQ(tokenize("a<b").types[1], TokenType::IsLess);
  EXPECT_EQ(tokenize("a>b").types[1], TokenType::IsGreater);
  EXPECT_EQ(tokenize("a<=b").types[1], TokenType::IsLessOrEqual);
  EXPECT_EQ(tokenize("a>=b").types[1], TokenType::IsGreaterOrEqual);
  EXPECT_EQ(tokenize("a==b").types[1], TokenType::IsEqual);
}

TEST(fn_lang_tokenize, Strings)
{
  StringRef str = "  \"hello\"  \"wor\\\"ld\" ";
  TokenizeResult result = tokenize(str);

  EXPECT_EQ(result.types.size(), 2);
  EXPECT_EQ(result.types[0], TokenType::StringLiteral);
  EXPECT_EQ(result.types[1], TokenType::StringLiteral);
  EXPECT_EQ(result.ranges[0].get(str), "\"hello\"");
  EXPECT_EQ(result.ranges[1].get(str), "\"wor\\\"ld\"");
}

TEST(fn_lang_tokenize, Asterix)
{
  EXPECT_EQ(tokenize("a*b").types[1], TokenType::Asterix);
  EXPECT_EQ(tokenize("a**b").types[1], TokenType::DoubleAsterix);
}

TEST(fn_lang_tokenize, Shift)
{
  EXPECT_EQ(tokenize("a<b").types[1], TokenType::IsLess);
  EXPECT_EQ(tokenize("a<<b").types[1], TokenType::DoubleLess);
  EXPECT_EQ(tokenize("a>b").types[1], TokenType::IsGreater);
  EXPECT_EQ(tokenize("a>>b").types[1], TokenType::DoubleRight);
}

}  // namespace blender::fn::lang::tests
