#include "llvm_types.hpp"

namespace FN {

	const char *LLVMTypeInfo::identifier_in_composition()
	{
		return "LLVM Type Info";
	}

	void LLVMTypeInfo::free_self(void *value)
	{
		LLVMTypeInfo *value_ = (LLVMTypeInfo *)value;
		delete value_;
	}

	llvm::Type *LLVMTypeInfo::get_type(llvm::LLVMContext &context)
	{
		if (!m_type_per_context.contains(&context)) {
			llvm::Type *type = this->create_type(context);
			m_type_per_context.add(&context, type);
		}
		return m_type_per_context.lookup(&context);
	}

	llvm::Value *LLVMTypeInfo::build_copy_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *value)
	{
		return value;
	}

	void LLVMTypeInfo::build_free_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *UNUSED(value))
	{
		return;
	}

};