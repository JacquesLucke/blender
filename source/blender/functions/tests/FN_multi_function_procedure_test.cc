/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn::tests {

TEST(multi_function_procedure, SimpleTest)
{
  CustomMF_SI_SI_SO<int, int, int> add_fn{"add", [](int a, int b) { return a + b; }};

  MFProcedure procedure;

  MFVariable &var1 = procedure.new_variable(MFDataType::ForSingle<int>(), "a");
  MFVariable &var2 = procedure.new_variable(MFDataType::ForSingle<int>(), "b");
  MFVariable &var3 = procedure.new_variable(MFDataType::ForSingle<int>(), "c");
  MFVariable &var4 = procedure.new_variable(MFDataType::ForSingle<int>(), "d");

  MFCallInstruction &add1_instr = procedure.new_call_instruction(add_fn, {&var1, &var2, &var3});
  MFCallInstruction &add2_instr = procedure.new_call_instruction(add_fn, {&var2, &var3, &var4});

  procedure.set_entry(add1_instr);
  add1_instr.set_next(&add2_instr);

  procedure.add_parameter(MFParamType::Input, var1);
  procedure.add_parameter(MFParamType::Input, var2);
  procedure.add_parameter(MFParamType::Output, var4);

  std::cout << "\n\n" << procedure.to_dot() << "\n\n";

  MFProcedureExecutor executor{"My Procedure", procedure};

  MFParamsBuilder params{executor, 1};
  MFContextBuilder context;

  executor.call({0}, params, context);
}

}  // namespace blender::fn::tests
