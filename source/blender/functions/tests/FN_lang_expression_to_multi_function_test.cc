/* Apache License, Version 2.0 */

#include "FN_lang_multi_function.hh"
#include "FN_multi_function_eval_utils.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

TEST(fn_lang_expression_to_multi_function, Test)
{
  MFSymbolTable symbols;
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "5", MFDataType::ForSingle<int>(), {}, {}, resources, symbols);

  const int result = mf_eval_1_SO<int>(fn);
  EXPECT_EQ(result, 5);
}

}  // namespace blender::fn::lang::tests
