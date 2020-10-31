/* Apache License, Version 2.0 */

#include "FN_lang_parse.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

TEST(fn_lang_parse, Test)
{
  LinearAllocator<> allocator;
  parse_expression("a+1<- length(float3(1, 2, 3))", allocator);
}

TEST(fn_lang_parse, Assignment)
{
  LinearAllocator<> allocator;
  const char *code = R"xx(
    a = 5;
    b = a + 2 - r;
    if (2 < 5) {
      a = 3;
      hello = "world";
    }
    if (a) a = 0;
  )xx";
  parse_program(code, allocator);
}

}  // namespace blender::fn::lang::tests
