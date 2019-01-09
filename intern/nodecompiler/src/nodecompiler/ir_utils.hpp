#pragma once

#include "core_types.hpp"

namespace LLVMNodeCompiler {

llvm::CallInst *callPointer(
	llvm::IRBuilder<> &builder,
	const void *pointer, llvm::FunctionType *type, llvm::ArrayRef<llvm::Value *> arguments);

llvm::Value *voidPtrToIR(llvm::IRBuilder<> &builder, const void *pointer);
llvm::Value *ptrToIR(llvm::IRBuilder<> &builder, const void *pointer, llvm::Type *type);

llvm::Type *getVoidPtrTy(llvm::IRBuilder<> &builder);
llvm::Type *getVoidPtrTy(llvm::LLVMContext &context);

} /* namespace LLVMNodeCompiler */