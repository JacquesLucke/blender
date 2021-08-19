/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn::tests {

TEST(multi_function_procedure, SimpleTest)
{
  CustomMF_SI_SI_SO<int, int, int> add_fn{"add", [](int a, int b) { return a + b; }};
  CustomMF_SM<int> add_10_fn{"add_10", [](int &a) { a += 10; }};

  MFProcedure procedure;

  MFVariable &var1 = procedure.new_variable(MFDataType::ForSingle<int>(), "a");
  MFVariable &var2 = procedure.new_variable(MFDataType::ForSingle<int>(), "b");
  MFVariable &var3 = procedure.new_variable(MFDataType::ForSingle<int>(), "c");
  MFVariable &var4 = procedure.new_variable(MFDataType::ForSingle<int>(), "d");

  MFCallInstruction &add1_instr = procedure.new_call_instruction(add_fn, {&var1, &var2, &var3});
  MFCallInstruction &add2_instr = procedure.new_call_instruction(add_fn, {&var2, &var3, &var4});
  MFCallInstruction &add3_instr = procedure.new_call_instruction(add_10_fn, {&var4});
  DestructInstructionChain destruction_chain = procedure.new_destruct_instructions(
      {&var1, &var2, &var3});

  procedure.set_entry(add1_instr);
  add1_instr.set_next(&add2_instr);
  add2_instr.set_next(&add3_instr);
  add3_instr.set_next(destruction_chain.first);

  procedure.add_parameter(MFParamType::Input, var1);
  procedure.add_parameter(MFParamType::Input, var2);
  procedure.add_parameter(MFParamType::Output, var4);

  MFProcedureExecutor executor{"My Procedure", procedure};

  MFParamsBuilder params{executor, 3};
  MFContextBuilder context;

  Array<int> input_array = {1, 2, 3};
  params.add_readonly_single_input(input_array.as_span());
  params.add_readonly_single_input_value(3);

  Array<int> output_array(3);
  params.add_uninitialized_single_output(output_array.as_mutable_span());

  executor.call(IndexRange(3), params, context);

  EXPECT_EQ(output_array[0], 17);
  EXPECT_EQ(output_array[1], 18);
  EXPECT_EQ(output_array[2], 19);
}

TEST(multi_function_procedure, BranchTest)
{
  CustomMF_SM<int> add_10_fn{"add_10", [](int &a) { a += 10; }};
  CustomMF_SM<int> add_100_fn{"add_100", [](int &a) { a += 100; }};

  MFProcedure procedure;
  MFVariable &a_var = procedure.new_variable(MFDataType::ForSingle<int>(), "a");
  MFVariable &cond_var = procedure.new_variable(MFDataType::ForSingle<bool>(), "cond");

  MFBranchInstruction &branch_instr = procedure.new_branch_instruction(&cond_var);
  MFCallInstruction &add_10_instr = procedure.new_call_instruction(add_10_fn, {&a_var});
  MFCallInstruction &add_100_instr = procedure.new_call_instruction(add_100_fn, {&a_var});
  DestructInstructionChain destruction_chain = procedure.new_destruct_instructions({&cond_var});

  procedure.set_entry(branch_instr);
  branch_instr.set_branch_false(&add_10_instr);
  branch_instr.set_branch_true(&add_100_instr);
  add_10_instr.set_next(destruction_chain.first);
  add_100_instr.set_next(destruction_chain.first);

  procedure.add_parameter(MFParamType::Mutable, a_var);
  procedure.add_parameter(MFParamType::Input, cond_var);

  MFProcedureExecutor procedure_fn{"Condition Test", procedure};
  MFParamsBuilder params(procedure_fn, 5);

  Array<int> values_a = {1, 5, 3, 6, 2};
  Array<bool> values_cond = {true, false, true, true, false};

  params.add_single_mutable(values_a.as_mutable_span());
  params.add_readonly_single_input(values_cond.as_span());

  MFContextBuilder context;
  procedure_fn.call({1, 2, 3, 4}, params, context);

  EXPECT_EQ(values_a[0], 1);
  EXPECT_EQ(values_a[1], 15);
  EXPECT_EQ(values_a[2], 103);
  EXPECT_EQ(values_a[3], 106);
  EXPECT_EQ(values_a[4], 12);
}

TEST(multi_function_procedure, SingleTest)
{
  int tot_evaluations = 0;
  CustomMF_SI_SO<int, int> add_10_fn{"add_10", [&](int a) {
                                       tot_evaluations++;
                                       return a + 10;
                                     }};

  MFProcedure procedure;
  MFVariable &in_var = procedure.new_variable(MFDataType::ForSingle<int>(), "in");
  MFVariable &out_var = procedure.new_variable(MFDataType::ForSingle<int>(), "out");

  MFCallInstruction &add_10_instr = procedure.new_call_instruction(add_10_fn, {&in_var, &out_var});
  DestructInstructionChain destruction_chain = procedure.new_destruct_instructions({&in_var});

  add_10_instr.set_next(destruction_chain.first);

  procedure.set_entry(add_10_instr);
  procedure.add_parameter(MFParamType::Input, in_var);
  procedure.add_parameter(MFParamType::Output, out_var);

  MFProcedureExecutor procedure_fn{"Single Test", procedure};
  MFParamsBuilder params{procedure_fn, 5};

  Array<int> values_out = {1, 2, 3, 4, 5};
  params.add_readonly_single_input_value(1);
  params.add_uninitialized_single_output(values_out.as_mutable_span());

  MFContextBuilder context;
  procedure_fn.call({0, 1, 3, 4}, params, context);

  EXPECT_EQ(values_out[0], 11);
  EXPECT_EQ(values_out[1], 11);
  EXPECT_EQ(values_out[2], 3);
  EXPECT_EQ(values_out[3], 11);
  EXPECT_EQ(values_out[4], 11);
  /* We expect only one evaluation, because the input is constant. */
  EXPECT_EQ(tot_evaluations, 1);
}

}  // namespace blender::fn::tests
