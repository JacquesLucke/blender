#include "builder.hpp"
#include "BLI_string.h"

namespace FN {

LLVMTypes CodeBuilder::types_of_values(LLVMValuesRef values)
{
  LLVMTypes types;
  for (llvm::Value *value : values) {
    types.append(value->getType());
  }
  return types;
}

LLVMTypes CodeBuilder::types_of_values(const LLVMValues &values)
{
  return this->types_of_values(LLVMValuesRef(values));
}

static llvm::Function *create_wrapper_function(llvm::Module *module,
                                               llvm::FunctionType *ftype,
                                               void *func_ptr,
                                               const char *name)
{
  llvm::Function *function = llvm::Function::Create(
      ftype, llvm::GlobalValue::LinkageTypes::InternalLinkage, name, module);

  llvm::BasicBlock *bb = llvm::BasicBlock::Create(module->getContext(), "entry", function);
  llvm::IRBuilder<> builder(bb);

  LLVMValues args;
  for (auto &arg : function->args()) {
    args.append(&arg);
  }

  llvm::Value *address_int = builder.getInt64((size_t)func_ptr);
  llvm::Value *address = builder.CreateIntToPtr(address_int, ftype->getPointerTo());
  llvm::Value *result = builder.CreateCall(address, to_llvm_array_ref(args));

  if (ftype->getReturnType() == builder.getVoidTy()) {
    builder.CreateRetVoid();
  }
  else {
    builder.CreateRet(result);
  }
  return function;
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            llvm::FunctionType *ftype,
                                            LLVMValuesRef args,
                                            const char *function_name)
{
  char name[64];
  BLI_snprintf(name, sizeof(name), "%s (%p)", function_name, func_ptr);

  llvm::Module *module = this->getModule();
  llvm::Function *wrapper_function = module->getFunction(name);
  if (wrapper_function == nullptr) {
    wrapper_function = create_wrapper_function(module, ftype, func_ptr, name);
  }

  return m_builder.CreateCall(wrapper_function, to_llvm_array_ref(args));
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            llvm::FunctionType *ftype,
                                            const LLVMValues &args,
                                            const char *function_name)
{
  return this->CreateCallPointer(func_ptr, ftype, LLVMValuesRef(args), function_name);
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            LLVMValuesRef args,
                                            llvm::Type *return_type,
                                            const char *function_name)
{
  LLVMTypes arg_types = this->types_of_values(args);
  llvm::FunctionType *ftype = llvm::FunctionType::get(
      return_type, to_llvm_array_ref(arg_types), false);
  return this->CreateCallPointer(func_ptr, ftype, args, function_name);
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            const LLVMValues &args,
                                            llvm::Type *return_type,
                                            const char *function_name)
{
  return this->CreateCallPointer(func_ptr, LLVMValuesRef(args), return_type, function_name);
}

static void simple_print(const char *str)
{
  std::cout << str << std::endl;
}

void CodeBuilder::CreatePrint(const char *str)
{
  this->CreateCallPointer(
      (void *)simple_print, {this->getVoidPtr((void *)str)}, this->getVoidTy());
}

static void simple_print_float(float value)
{
  std::cout << value << std::endl;
}

void CodeBuilder::CreatePrintFloat(llvm::Value *value)
{
  BLI_assert(value->getType()->isFloatTy());
  this->CreateCallPointer((void *)simple_print_float, {value}, this->getVoidTy());
}

} /* namespace FN */
