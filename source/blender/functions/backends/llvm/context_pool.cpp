#include "context_pool.hpp"
#include "BLI_object_pool.h"

namespace FN {

static BLI::ThreadSafeObjectPool<llvm::LLVMContext> contexts;

llvm::LLVMContext *aquire_llvm_context()
{
  return contexts.aquire();
}

void release_llvm_context(llvm::LLVMContext *context)
{
  contexts.release(context);
}

}  // namespace FN
