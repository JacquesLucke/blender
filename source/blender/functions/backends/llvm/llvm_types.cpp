#include "llvm_types.hpp"
#include "ir_utils.hpp"

namespace FN {

	/******************** LLVMTypeInfo ********************/

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


	/******************** SimpleLLVMTypeInfo ********************/

	llvm::Value *SimpleLLVMTypeInfo::build_copy_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *value) const
	{
		return value;
	}

	void SimpleLLVMTypeInfo::build_free_ir(
		llvm::IRBuilder<> &UNUSED(builder),
		llvm::Value *UNUSED(value)) const
	{
		return;
	}

	void SimpleLLVMTypeInfo::build_store_ir__relocate(
		llvm::IRBuilder<> &builder,
		llvm::Value *value,
		llvm::Value *byte_addr) const
	{
		llvm::Type *ptr_type = value->getType()->getPointerTo();
		llvm::Value *addr = builder.CreatePointerCast(byte_addr, ptr_type);
		builder.CreateStore(value, addr, false);
	}

	llvm::Value *SimpleLLVMTypeInfo::build_load_ir__copy(
		llvm::IRBuilder<> &builder,
		llvm::Value *byte_addr) const
	{
		llvm::Type *ptr_type = this->get_type_ptr(builder.getContext());
		llvm::Value *addr = builder.CreatePointerCast(byte_addr, ptr_type);
		return builder.CreateLoad(addr);
	}

	llvm::Value *SimpleLLVMTypeInfo::build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const
	{
		return this->build_load_ir__copy(builder, byte_addr);
	}


	/******************** PointerLLVMTypeInfo ********************/

	void *PointerLLVMTypeInfo::copy_value(
		PointerLLVMTypeInfo *info, void *value)
	{
		return info->m_copy_func(value);
	}

	void PointerLLVMTypeInfo::free_value(
		PointerLLVMTypeInfo *info, void *value)
	{
		info->m_free_func(value);
	}

	void *PointerLLVMTypeInfo::default_value(
		PointerLLVMTypeInfo *info)
	{
		return info->m_default_func();
	}

	llvm::Value *PointerLLVMTypeInfo::build_copy_ir(
		llvm::IRBuilder<> &builder,
		llvm::Value *value) const
	{
		auto *void_ptr_ty = builder.getVoidTy()->getPointerTo();
		auto *copy_ftype = llvm::FunctionType::get(
			void_ptr_ty, {void_ptr_ty, void_ptr_ty}, false);

		return call_pointer(
			builder,
			(void *)PointerLLVMTypeInfo::copy_value,
			copy_ftype,
			{void_ptr_to_ir(builder, (void *)this), value});
	}

	void PointerLLVMTypeInfo::build_free_ir(
		llvm::IRBuilder<> &builder,
		llvm::Value *value) const
	{
		auto *void_ty = builder.getVoidTy();
		auto *void_ptr_ty = void_ty->getPointerTo();
		auto free_ftype = llvm::FunctionType::get(
			void_ty, {void_ptr_ty, void_ptr_ty}, false);

		call_pointer(
			builder,
			(void *)PointerLLVMTypeInfo::free_value,
			free_ftype,
			{void_ptr_to_ir(builder, (void *)this), value});
	}

	void PointerLLVMTypeInfo::build_store_ir__relocate(
		llvm::IRBuilder<> &builder,
		llvm::Value *value,
		llvm::Value *byte_addr) const
	{
		auto *void_ty = builder.getVoidTy();
		auto *void_ptr_ty = void_ty->getPointerTo();
		auto *void_ptr_ptr_ty = void_ptr_ty->getPointerTo();

		auto *addr = builder.CreatePointerCast(byte_addr, void_ptr_ptr_ty);
		builder.CreateStore(value, addr);
	}

	llvm::Value *PointerLLVMTypeInfo::build_load_ir__copy(
		llvm::IRBuilder<> &builder,
		llvm::Value *byte_addr) const
	{
		auto *value = this->build_load_ir__relocate(builder, byte_addr);
		this->build_copy_ir(builder, value);
		return value;
	}

	llvm::Value *PointerLLVMTypeInfo::build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const
	{
		auto *void_ty = builder.getVoidTy();
		auto *void_ptr_ty = void_ty->getPointerTo();
		auto *void_ptr_ptr_ty = void_ptr_ty->getPointerTo();

		auto *addr = builder.CreatePointerCast(byte_addr, void_ptr_ptr_ty);
		return builder.CreateLoad(addr);
	}
};