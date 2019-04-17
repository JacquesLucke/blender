#include "initialize.hpp"

#include <llvm/Support/TargetSelect.h>

namespace FN {

void initialize_llvm()
{
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
}

} /* namespace FN */
