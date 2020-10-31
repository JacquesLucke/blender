/* Apache License, Version 2.0 */

#include "FN_lang_multi_function.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

TEST(fn_lang_expression_to_multi_function, Test)
{
  MFSymbolTable symbols;
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "5", MFDataType::ForSingle<int>(), {}, {}, resources, symbols);

  MFParamsBuilder params{fn, 1};
  MFContextBuilder context;
  int value = 0;
  params.add_uninitialized_single_output(&value);
  fn.call({0}, params, context);

  EXPECT_EQ(value, 5);
}

}  // namespace blender::fn::lang::tests
