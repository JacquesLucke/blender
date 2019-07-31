#pragma once

#include "FN_core.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Functions {

class ArrayExecution {
 protected:
  SharedFunction m_function;
  Vector<uint> m_input_sizes;
  Vector<uint> m_output_sizes;

 public:
  ArrayExecution(SharedFunction function);
  virtual ~ArrayExecution();

  virtual void call(ArrayRef<uint> indices,
                    ArrayRef<void *> input_buffers,
                    ArrayRef<void *> output_buffers,
                    ExecutionContext &execution_context) = 0;
};

class TupleCallArrayExecution : public ArrayExecution {
 public:
  TupleCallArrayExecution(SharedFunction function);

  void call(ArrayRef<uint> indices,
            ArrayRef<void *> input_buffers,
            ArrayRef<void *> output_buffers,
            ExecutionContext &execution_context) override;
};

class LLVMArrayExecution : public ArrayExecution {
 private:
  std::unique_ptr<CompiledLLVM> m_compiled_function;

 public:
  LLVMArrayExecution(SharedFunction function);

  void call(ArrayRef<uint> indices,
            ArrayRef<void *> input_buffers,
            ArrayRef<void *> output_buffers,
            ExecutionContext &execution_context) override;

 private:
  void compile();
  llvm::Function *build_function_ir(llvm::Module *module);
};

}  // namespace Functions
}  // namespace FN
