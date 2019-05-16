#include "function.hpp"

namespace FN {

void Function::print() const
{
  std::cout << "Function: " << this->name() << std::endl;
  this->signature().print("  ");
}

/* Function builder
 ************************************/

FunctionBuilder::FunctionBuilder()
{
}

void FunctionBuilder::add_input(StringRef name, SharedType &type)
{
  m_inputs.append(InputParameter(name, type));
}

void FunctionBuilder::add_output(StringRef name, SharedType &type)
{
  m_outputs.append(OutputParameter(name, type));
}

SharedFunction FunctionBuilder::build(StringRef function_name)
{
  return SharedFunction::New(function_name, Signature(m_inputs, m_outputs));
}

} /* namespace FN */
