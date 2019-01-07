#pragma once

#include "core_types.hpp"
#include "ir_utils.hpp"

namespace LLVMNodeCompiler {

template<typename T>
class PointerType : public Type {
private:
	static void *copy_(PointerType<T> *self, void *value)
	{ return (void *)self->copy((T *)value); }
	static void free_(PointerType<T> *self, void *value)
	{ self->free((T *)value); }

public:
	virtual T *copy(T *value) = 0;
	virtual void free(T *value) = 0;

	llvm::Value *buildCopyIR(llvm::IRBuilder<> &builder, llvm::Value *value)
	{
		llvm::Type *void_ptr = getVoidPtrTy(builder);

		llvm::FunctionType *ftype = llvm::FunctionType::get(
			void_ptr, { void_ptr, void_ptr }, false);

		llvm::Value *this_pointer = voidPtrToIR(builder, this);
		return callPointer(builder, (void *)copy_, ftype, { this_pointer, value });
	}

	void buildFreeIR(llvm::IRBuilder<> &builder, llvm::Value *value)
	{
		llvm::Type *void_ptr = getVoidPtrTy(builder);

		llvm::FunctionType *ftype = llvm::FunctionType::get(
			builder.getVoidTy(), {void_ptr, void_ptr}, false);

		llvm::Value *this_pointer = voidPtrToIR(builder, this);
		callPointer(builder, (void *)free_, ftype, { this_pointer, value });
	}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return getVoidPtrTy(context);
	}
};

} /* namespace LLVMNodeCompiler */