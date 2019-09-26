#include "function.hpp"

namespace FN {

/* Function
 **********************************************/

Function::Function(StringRefNull name,
                   ArrayRef<StringRefNull> input_names,
                   ArrayRef<Type *> input_types,
                   ArrayRef<StringRefNull> output_names,
                   ArrayRef<Type *> output_types,
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
  for (uint i = 0; i < ARRAY_SIZE(m_bodies); i++) {
    if (m_bodies[i] != nullptr) {
      delete m_bodies[i];
    }
  }

  if (m_resources) {
    m_resources->print(m_name);
  }

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

/* Function Body
 ***********************************************/

void FunctionBody::owner_init_post()
{
}

FunctionBody::~FunctionBody()
{
}

} /* namespace FN */
