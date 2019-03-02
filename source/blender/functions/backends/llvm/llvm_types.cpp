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

	llvm::Type *LLVMTypeInfo::get_type(
		llvm::LLVMContext &context) const
	{
		if (!m_type_per_context.contains(&context)) {
			llvm::Type *type = this->create_type(context);
			m_type_per_context.add(&context, type);
		}
		return m_type_per_context.lookup(&context);
	}

	llvm::Type *LLVMTypeInfo::get_type_ptr(
		llvm::LLVMContext &context) const
	{
		return this->get_type(context)->getPointerTo();
	}

	llvm::Value *LLVMTypeInfo::build_copy_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *value) const
	{
		return value;
	}

	void LLVMTypeInfo::build_free_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *UNUSED(value)) const
	{
		return;
	}

	void LLVMTypeInfo::build_store_ir__relocate(
		llvm::IRBuilder<> &builder,
		llvm::Value *value,
		llvm::Value *byte_addr) const
	{
		llvm::Type *ptr_type = value->getType()->getPointerTo();
		llvm::Value *addr = builder.CreatePointerCast(byte_addr, ptr_type);
		builder.CreateStore(value, addr, false);
	}

	llvm::Value *LLVMTypeInfo::build_load_ir__copy(
		llvm::IRBuilder<> &builder,
		llvm::Value *byte_addr) const
	{
		llvm::Type *ptr_type = this->get_type_ptr(builder.getContext());
		llvm::Value *addr = builder.CreatePointerCast(byte_addr, ptr_type);
		return builder.CreateLoad(addr);
	}

};