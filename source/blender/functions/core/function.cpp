#include "function.hpp"

namespace FN {

void Function::print() const
{
  std::cout << "Function: " << this->name() << std::endl;
  std::cout << "  Inputs:" << std::endl;
  for (InputParameter &param : m_signature.inputs()) {
    std::cout << "    ";
    param.print();
    std::cout << std::endl;
  }
  std::cout << "  Outputs:" << std::endl;
  for (OutputParameter &param : m_signature.outputs()) {
    std::cout << "    ";
    param.print();
    std::cout << std::endl;
  }
}

/* Function builder
 ************************************/

FunctionBuilder::FunctionBuilder()
{
}

void FunctionBuilder::add_input(StringRef name, SharedType &type)
{
  m_input_names.append(name.to_std_string());
  m_input_types.append(type);
}

void FunctionBuilder::add_output(StringRef name, SharedType &type)
{
  m_output_names.append(name.to_std_string());
  m_output_types.append(type);
}

SharedFunction FunctionBuilder::build(StringRef function_name)
{
  InputParameters inputs;
  for (uint i = 0; i < m_input_names.size(); i++) {
    inputs.append(InputParameter(m_input_names[i], m_input_types[i]));
  }
  OutputParameters outputs;
  for (uint i = 0; i < m_output_names.size(); i++) {
    outputs.append(OutputParameter(m_output_names[i], m_output_types[i]));
  }
  return SharedFunction::New(function_name, Signature(inputs, outputs));
}

} /* namespace FN */
