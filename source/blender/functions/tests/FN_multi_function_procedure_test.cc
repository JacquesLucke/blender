/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn::tests {

TEST(multi_function_procedure, SimpleTest)
{
  CustomMF_SI_SI_SO<int, int, int> add_fn{"add", [](int a, int b) { return a + b; }};
  CustomMF_SM<int> add_10_fn{"add_10", [](int &a) { a += 10; }};

  MFProcedure procedure;
  MFProcedureBuilder builder{procedure};

  MFVariable *var1 = &builder.add_single_input_parameter<int>();
  MFVariable *var2 = &builder.add_single_input_parameter<int>();
  auto [var3] = builder.insert_call_with_new_variables<1>(add_fn, {var1, var2});
  auto [var4] = builder.insert_call_with_new_variables<1>(add_fn, {var2, var3});
  builder.insert_call(add_10_fn, {var4});
  builder.insert_destruct({var1, var2, var3});
  builder.add_output_parameter(*var4);

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
  MFProcedureBuilder builder{procedure};

  MFVariable *var1 = &builder.add_single_mutable_parameter<int>();
  MFVariable *var2 = &builder.add_single_input_parameter<bool>();

  MFProcedureBuilderBranch branch = builder.insert_branch(*var2);
  branch.branch_false.insert_call(add_10_fn, {var1});
  branch.branch_true.insert_call(add_100_fn, {var1});
  builder.set_cursor_after_branch(branch);
  builder.insert_call(add_10_fn, {var1});
  builder.insert_destruct({var2});

  MFProcedureExecutor procedure_fn{"Condition Test", procedure};
  MFParamsBuilder params(procedure_fn, 5);

  Array<int> values_a = {1, 5, 3, 6, 2};
  Array<bool> values_cond = {true, false, true, true, false};

  params.add_single_mutable(values_a.as_mutable_span());
  params.add_readonly_single_input(values_cond.as_span());

  MFContextBuilder context;
  procedure_fn.call({1, 2, 3, 4}, params, context);

  EXPECT_EQ(values_a[0], 1);
  EXPECT_EQ(values_a[1], 25);
  EXPECT_EQ(values_a[2], 113);
  EXPECT_EQ(values_a[3], 116);
  EXPECT_EQ(values_a[4], 22);
}

TEST(multi_function_procedure, EvaluateOne)
{
  int tot_evaluations = 0;
  CustomMF_SI_SO<int, int> add_10_fn{"add_10", [&](int a) {
                                       tot_evaluations++;
                                       return a + 10;
                                     }};

  MFProcedure procedure;
  MFProcedureBuilder builder{procedure};

  MFVariable *var1 = &builder.add_single_input_parameter<int>();
  auto [var2] = builder.insert_call_with_new_variables<1>(add_10_fn, {var1});
  builder.insert_destruct(*var1);
  builder.add_output_parameter(*var2);

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
