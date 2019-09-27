#pragma once

#include "FN_core.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Functions {

class ArrayExecution {
 protected:
  Function &m_function;
  Vector<uint> m_input_sizes;
  Vector<uint> m_output_sizes;

 public:
  ArrayExecution(Function &function);
  virtual ~ArrayExecution();

  virtual void call(ArrayRef<uint> indices,
                    ArrayRef<void *> input_buffers,
                    MutableArrayRef<void *> output_buffers,
                    ExecutionContext &execution_context) = 0;
};

std::unique_ptr<ArrayExecution> get_tuple_call_array_execution(Function &function);
std::unique_ptr<ArrayExecution> get_precompiled_array_execution(Function &function);

}  // namespace Functions
}  // namespace FN
