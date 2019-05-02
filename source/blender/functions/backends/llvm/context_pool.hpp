#pragma once

#include "llvm/IR/LLVMContext.h"

namespace FN {

llvm::LLVMContext *aquire_llvm_context();
void release_llvm_context(llvm::LLVMContext *context);

}  // namespace FN
