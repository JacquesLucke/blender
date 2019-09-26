#include "FN_core.hpp"

namespace FN {

FunctionBuilder::FunctionBuilder()
{
}

void FunctionBuilder::add_input(StringRef name, Type *type)
{
  BLI_assert(type != nullptr);
  auto ref = m_strings_builder.add(name);
  m_input_names.append(ref);
  m_input_types.append(type);
}

void FunctionBuilder::add_output(StringRef name, Type *type)
{
  BLI_assert(type != nullptr);
  auto ref = m_strings_builder.add(name);
  m_output_names.append(ref);
  m_output_types.append(type);
}

void FunctionBuilder::add_inputs(const DataGraph &data_graph, ArrayRef<DataSocket> sockets)
{
  for (DataSocket socket : sockets) {
    StringRef name = data_graph.name_of_socket(socket);
    Type *type = data_graph.type_of_socket(socket);
    this->add_input(name, type);
  }
}

void FunctionBuilder::add_outputs(const DataGraph &data_graph, ArrayRef<DataSocket> sockets)
{
  for (DataSocket socket : sockets) {
    StringRef name = data_graph.name_of_socket(socket);
    Type *type = data_graph.type_of_socket(socket);
    this->add_output(name, type);
  }
}

SharedFunction FunctionBuilder::build(StringRef function_name)
{
  auto name_ref = m_strings_builder.add(function_name);
  char *strings = m_strings_builder.build();

  Vector<StringRefNull> input_names;
  Vector<StringRefNull> output_names;
  input_names.reserve(m_input_names.size());
  output_names.reserve(m_output_names.size());

  for (ChainedStringRef name : m_input_names) {
    input_names.append(name.to_string_ref(strings));
  }
  for (ChainedStringRef name : m_output_names) {
    output_names.append(name.to_string_ref(strings));
  }

  return SharedFunction::New(name_ref.to_string_ref(strings),
                             input_names,
                             m_input_types,
                             output_names,
                             m_output_types,
                             strings);
}

}  // namespace FN
