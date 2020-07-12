/* Apache License, Version 2.0 */

#include "FN_expression_parser.hh"

#include "testing/testing.h"

namespace blender::fn {

TEST(parser, Test)
{
  LinearAllocator<> allocator;
  parse_expression("a+1<- length(float3(1, 2, 3))", allocator);
}

}  // namespace blender::fn
