#include "builder.hpp"

namespace FN {

LLVMTypes CodeBuilder::types_of_values(const LLVMValues &values)
{
  LLVMTypes types;
  for (llvm::Value *value : values) {
    types.append(value->getType());
  }
  return types;
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            llvm::FunctionType *ftype,
                                            const LLVMValues &args)
{
  auto address_int = m_builder.getInt64((size_t)func_ptr);
  auto address = m_builder.CreateIntToPtr(address_int, ftype->getPointerTo());
  return m_builder.CreateCall(address, to_array_ref(args));
}

void CodeBuilder::CreateCallPointer_NoReturnValue(void *func_ptr, const LLVMValues &args)
{
  LLVMTypes arg_types = this->types_of_values(args);

  llvm::FunctionType *ftype = llvm::FunctionType::get(
      this->getVoidTy(), to_array_ref(arg_types), false);

  this->CreateCallPointer(func_ptr, ftype, args);
}

} /* namespace FN */
