#pragma once

#include "BLI_chained_strings.h"

#include "function.hpp"
#include "data_graph.hpp"

namespace FN {

using BLI::ChainedStringRef;
using BLI::ChainedStringsBuilder;

class FunctionBuilder {
 private:
  ChainedStringsBuilder m_strings_builder;
  Vector<ChainedStringRef> m_input_names;
  Vector<Type *> m_input_types;
  Vector<ChainedStringRef> m_output_names;
  Vector<Type *> m_output_types;

 public:
  FunctionBuilder();

  /**
   * Add an input to the function with the given name and type.
   */
  void add_input(StringRef input_name, Type *type);

  /**
   * Add an output to the function with the given name and type.
   */
  void add_output(StringRef output_name, Type *type);

  /**
   * Add multiple inputs. The names and types are taken from the sockets.
   */
  void add_inputs(const DataGraph &data_graph, ArrayRef<DataSocket> sockets);

  /**
   * Add multiple outputs. The names and types are taken from the sockets.
   */
  void add_outputs(const DataGraph &data_graph, ArrayRef<DataSocket> sockets);

  /**
   * Create a new function with the given name and all the inputs and outputs previously added.
   */
  std::unique_ptr<Function> build(StringRef function_name);
};

}  // namespace FN
