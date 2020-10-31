/* Apache License, Version 2.0 */

#include "FN_lang_parse.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

TEST(fn_lang_parse, Test)
{
  LinearAllocator<> allocator;
  parse_expression("a+1<- length(float3(1, 2, 3))", allocator);
}

}  // namespace blender::fn::lang::tests
