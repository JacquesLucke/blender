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

  MFCallInstruction &add_instr = procedure.new_call_instruction(add_fn);
  add_instr.set_param_variable(0, &var1);
  add_instr.set_param_variable(1, &var2);
  add_instr.set_param_variable(2, &var3);

  procedure.add_parameter(MFParamType::Input, var1);
  procedure.add_parameter(MFParamType::Input, var2);
  procedure.add_parameter(MFParamType::Output, var3);

  procedure.set_entry(add_instr);

  std::cout << "\n\n" << procedure.to_dot() << "\n\n";

  MFProcedureExecutor executor{"My Procedure", procedure};

  MFParamsBuilder params{executor, 1};
  MFContextBuilder context;

  executor.call({0}, params, context);
}

}  // namespace blender::fn::tests
