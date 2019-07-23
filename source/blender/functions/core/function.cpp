#include "function.hpp"

namespace FN {

/* Function
 **********************************************/

Function::Function(ChainedStringRef name,
                   ArrayRef<ChainedStringRef> input_names,
                   ArrayRef<SharedType> input_types,
                   ArrayRef<ChainedStringRef> output_names,
                   ArrayRef<SharedType> output_types,
                   const char *strings)
    : m_name(name),
      m_input_names(input_names),
      m_input_types(input_types),
      m_output_names(output_names),
      m_output_types(output_types),
      m_strings(strings)
{
  BLI_assert(m_input_names.size() == m_input_types.size());
  BLI_assert(m_output_names.size() == m_output_types.size());
}

Function::~Function()
{
  MEM_freeN((void *)m_strings);
  for (uint i = 0; i < ARRAY_SIZE(m_bodies); i++) {
    if (m_bodies[i] != nullptr) {
      delete m_bodies[i];
    }
  }
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

/* Function Body
 ***********************************************/

void FunctionBody::owner_init_post()
{
}

FunctionBody::~FunctionBody()
{
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
