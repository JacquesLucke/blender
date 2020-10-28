/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_script.hh"

namespace blender::fn::script::tests {

TEST(multi_function_script, Test1)
{
  CustomMF_SI_SO<int, int> add_10_fn("add 20", [](int value) { return value + 20; });

  MFRegister reg1{MFDataType::ForSingle<int>()};
  MFRegister reg2{MFDataType::ForSingle<int>()};
  MFRegister reg3{MFDataType::ForSingle<int>()};

  MFFunctionStep step1;
  step1.function = &add_10_fn;
  step1.registers = {&reg1, &reg2};

  MFFunctionStep step2;
  step2.function = &add_10_fn;
  step2.registers = {&reg2, &reg3};

  MFScript script;
  script.steps = {&step1, &step2};
  script.registers = {&reg1, &reg2, &reg3};
  script.input_registers = {&reg1};
  script.output_registers = {&reg3};

  MFScriptEvaluator script_fn{script};

  MFParamsBuilder params{script_fn, 1};
  MFContextBuilder context;

  int input_value = 13;
  int output_value;
  params.add_readonly_single_input(&input_value);
  params.add_uninitialized_single_output(&output_value);

  script_fn.call({0}, params, context);

  EXPECT_EQ(output_value, 53);
}

}  // namespace blender::fn::script::tests
