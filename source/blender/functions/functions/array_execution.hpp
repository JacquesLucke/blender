#pragma once

#include "FN_core.hpp"
#include "FN_tuple_call.hpp"

namespace FN {
namespace Functions {

class TupleCallArrayExecution {
 private:
  SharedFunction m_function;
  Vector<uint> m_input_sizes;
  Vector<uint> m_output_sizes;

 public:
  TupleCallArrayExecution(SharedFunction function);

  void call(ArrayRef<uint> indices,
            ArrayRef<void *> input_buffers,
            ArrayRef<void *> output_buffers,
            ExecutionContext &execution_context);
};

}  // namespace Functions
}  // namespace FN
