#pragma once

#include <memory>

namespace llvm {
class ExecutionEngine;
class Module;
class Function;
}  // namespace llvm

namespace FN {

class CompiledLLVM {
 private:
  llvm::ExecutionEngine *m_engine;
  void *m_func_ptr;

  CompiledLLVM() = default;

 public:
  ~CompiledLLVM();

  static std::unique_ptr<CompiledLLVM> FromIR(llvm::Module *module, llvm::Function *main_function);

  void *function_ptr() const
  {
    return m_func_ptr;
  }
};

} /* namespace FN */
