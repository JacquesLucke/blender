#include "core.hpp"

namespace LLVMNodeCompiler {

llvm::Type *Type::getLLVMType(llvm::LLVMContext &context)
{
	if (!this->typePerContext.contains(&context)) {
		llvm::Type *type = this->createLLVMType(context);
		this->typePerContext.add(&context, type);
	}
	return this->typePerContext.lookup(&context);
}

llvm::Value *Type::buildCopyIR(
	llvm::IRBuilder<> &UNUSED(builder),
	llvm::Value *value)
{
	return value;
}

void Type::buildFreeIR(
	llvm::IRBuilder<> &UNUSED(builder),
	llvm::Value *UNUSED(value))
{
	return;
}

} /* namespace LLVMNodeCompiler */