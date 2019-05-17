#include "function.hpp"

namespace FN {

void Function::print()
{
  std::cout << "Function: " << this->name() << std::endl;
  std::cout << "  Inputs:" << std::endl;
  for (uint i = 0; i < this->input_amount(); i++) {
    std::cout << "    " << this->input_type(i)->name() << " - " << this->input_name(i)
              << std::endl;
  }
  std::cout << "  Outputs:" << std::endl;
  for (uint i = 0; i < this->output_amount(); i++) {
    std::cout << "    " << this->output_type(i)->name() << " - " << this->output_name(i)
              << std::endl;
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
  return SharedFunction::New(
      function_name, m_input_names, m_input_types, m_output_names, m_output_types);
}

} /* namespace FN */
