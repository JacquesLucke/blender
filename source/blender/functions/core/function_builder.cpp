#include "FN_core.hpp"

namespace FN {

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

void FunctionBuilder::add_inputs(const SharedDataFlowGraph &graph, ArrayRef<DFGraphSocket> sockets)
{
  for (DFGraphSocket socket : sockets) {
    StringRef name = graph->name_of_socket(socket);
    SharedType &type = graph->type_of_socket(socket);
    this->add_input(name, type);
  }
}

void FunctionBuilder::add_outputs(const SharedDataFlowGraph &graph,
                                  ArrayRef<DFGraphSocket> sockets)
{
  for (DFGraphSocket socket : sockets) {
    StringRef name = graph->name_of_socket(socket);
    SharedType &type = graph->type_of_socket(socket);
    this->add_output(name, type);
  }
}

SharedFunction FunctionBuilder::build(StringRef function_name)
{
  auto name_ref = m_strings_builder.add(function_name);
  char *strings = m_strings_builder.build();
  return SharedFunction::New(
      name_ref, m_input_names, m_input_types, m_output_names, m_output_types, strings);
}

}  // namespace FN
