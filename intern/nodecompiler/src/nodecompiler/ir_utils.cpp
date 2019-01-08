#include "core.hpp"

namespace LLVMNodeCompiler {

llvm::CallInst *callPointer(
	llvm::IRBuilder<> &builder,
	void *pointer, llvm::FunctionType *type, llvm::ArrayRef<llvm::Value *> arguments)
{
	auto address_int = builder.getInt64((size_t)pointer);
	auto address = builder.CreateIntToPtr(address_int, type->getPointerTo());
	return builder.CreateCall(address, arguments);
}

llvm::Value *voidPtrToIR(llvm::IRBuilder<> &builder, void *pointer)
{
	return ptrToIR(builder, pointer, getVoidPtrTy(builder));
}

llvm::Value *ptrToIR(llvm::IRBuilder<> &builder, void *pointer, llvm::Type *type)
{
	return builder.CreateIntToPtr(builder.getInt64((size_t)pointer), type);
}

llvm::Type *getVoidPtrTy(llvm::IRBuilder<> &builder)
{
	return builder.getVoidTy()->getPointerTo();
}

llvm::Type *getVoidPtrTy(llvm::LLVMContext &context)
{
	return llvm::Type::getVoidTy(context)->getPointerTo();
}

} /* namespace LLVMNodeCompiler */