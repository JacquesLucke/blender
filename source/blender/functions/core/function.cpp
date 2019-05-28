#include "function.hpp"

namespace FN {

Function::~Function()
{
  MEM_freeN((void *)m_strings);
}

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
  auto ref = m_strings_builder.add(name);
  m_input_names.append(ref);
  m_input_types.append(type);
}

void FunctionBuilder::add_output(StringRef name, SharedType &type)
{
  auto ref = m_strings_builder.add(name);
  m_output_names.append(ref);
  m_output_types.append(type);
}

SharedFunction FunctionBuilder::build(StringRef function_name)
{
  auto name_ref = m_strings_builder.add(function_name);
  char *strings = m_strings_builder.build();
  return SharedFunction::New(
      name_ref, m_input_names, m_input_types, m_output_names, m_output_types, strings);
}

} /* namespace FN */
