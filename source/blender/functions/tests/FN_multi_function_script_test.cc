/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_script.hh"

namespace blender::fn::script::tests {

TEST(multi_function_script, Test1)
{
  CustomMF_SI_SO<int, int> add_10_fn("add 10", [](int value) { return value + 10; });
  CustomMF_SI_SO<int, int> add_50_fn("add 50", [](int value) { return value + 50; });
  CustomMF_SI_SO<int, bool> smaller_than_20_fn("smaller than 20",
                                               [](int value) { return value < 20; });

  MFRegister reg1{MFDataType::ForSingle<int>()};
  MFRegister reg2{MFDataType::ForSingle<int>()};
  MFRegister reg3{MFDataType::ForSingle<bool>()};

  MFCallInstruction compare_instr;
  compare_instr.function = &smaller_than_20_fn;
  compare_instr.registers = {&reg1, &reg3};

  MFBranchInstruction branch_instr;
  branch_instr.condition = &reg3;

  MFCallInstruction add_10_instr;
  add_10_instr.function = &add_10_fn;
  add_10_instr.registers = {&reg1, &reg2};

  MFCallInstruction add_50_instr;
  add_50_instr.function = &add_50_fn;
  add_50_instr.registers = {&reg1, &reg2};

  compare_instr.next = &branch_instr;
  branch_instr.true_instruction = &add_10_instr;
  branch_instr.false_instruction = &add_50_instr;

  MFScript script;
  script.entry = &compare_instr;
  script.registers = {&reg1, &reg2, &reg3};
  script.input_registers = {&reg1};
  script.output_registers = {&reg2};

  MFScriptEvaluator script_fn{script};

  Array<int> input_values = {16, 17, 18, 19, 20, 21, 20};
  Array<int> output_values(input_values.size());

  MFParamsBuilder params{script_fn, input_values.size()};
  MFContextBuilder context;
  params.add_readonly_single_input(input_values.as_span());
  params.add_uninitialized_single_output(output_values.as_mutable_span());

  script_fn.call(input_values.index_range(), params, context);

  for (int value : output_values) {
    std::cout << value << "\n";
  }
}

}  // namespace blender::fn::script::tests
