#include "llvm_types.hpp"

namespace FN {

	/******************** LLVMTypeInfo ********************/

	BLI_COMPOSITION_IMPLEMENTATION(LLVMTypeInfo);


	/******************** SimpleLLVMTypeInfo ********************/

	llvm::Value *SimpleLLVMTypeInfo::build_copy_ir(
		CodeBuilder &UNUSED(builder),
		llvm::Value *value) const
	{
		return value;
	}

	void SimpleLLVMTypeInfo::build_free_ir(
		CodeBuilder &UNUSED(builder),
		llvm::Value *UNUSED(value)) const
	{
		return;
	}

	void SimpleLLVMTypeInfo::build_store_ir__relocate(
		CodeBuilder &builder,
		llvm::Value *value,
		llvm::Value *byte_addr) const
	{
		llvm::Type *type = value->getType();
		llvm::Value *addr = builder.CastToPointerOf(byte_addr, type);
		builder.CreateStore(value, addr);
	}

	llvm::Value *SimpleLLVMTypeInfo::build_load_ir__copy(
		CodeBuilder &builder,
		llvm::Value *byte_addr) const
	{
		llvm::Type *type = this->get_type(builder.getContext());
		llvm::Value *addr = builder.CastToPointerOf(byte_addr, type);
		return builder.CreateLoad(addr);
	}

	llvm::Value *SimpleLLVMTypeInfo::build_load_ir__relocate(
			CodeBuilder &builder,
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
		CodeBuilder &builder,
		llvm::Value *value) const
	{
		auto *void_ptr_ty = builder.getVoidPtrTy();
		auto *copy_ftype = llvm::FunctionType::get(
			void_ptr_ty, {void_ptr_ty, void_ptr_ty}, false);

		return builder.CreateCallPointer(
			(void *)PointerLLVMTypeInfo::copy_value,
			copy_ftype,
			{builder.getVoidPtr((void *)this), value});
	}

	void PointerLLVMTypeInfo::build_free_ir(
		CodeBuilder &builder,
		llvm::Value *value) const
	{
		builder.CreateCallPointer_NoReturnValue(
			(void *)PointerLLVMTypeInfo::free_value,
			{builder.getVoidPtr((void *)this), value});
	}

	void PointerLLVMTypeInfo::build_store_ir__relocate(
		CodeBuilder &builder,
		llvm::Value *value,
		llvm::Value *byte_addr) const
	{
		auto *addr = builder.CastToPointerOf(byte_addr, builder.getVoidPtrTy());
		builder.CreateStore(value, addr);
	}

	llvm::Value *PointerLLVMTypeInfo::build_load_ir__copy(
		CodeBuilder &builder,
		llvm::Value *byte_addr) const
	{
		auto *value = this->build_load_ir__relocate(builder, byte_addr);
		this->build_copy_ir(builder, value);
		return value;
	}

	llvm::Value *PointerLLVMTypeInfo::build_load_ir__relocate(
			CodeBuilder &builder,
			llvm::Value *byte_addr) const
	{
		auto *addr = builder.CastToPointerOf(byte_addr, builder.getVoidPtrTy());
		return builder.CreateLoad(addr);
	}


	/* Utilities
	 ******************************************/

	LLVMTypes types_of_type_infos(
		const SmallVector<LLVMTypeInfo *> &type_infos,
		llvm::LLVMContext &context)
	{
		LLVMTypes types;
		for (auto info : type_infos) {
			types.append(info->get_type(context));
		}
		return types;
	}

	llvm::FunctionType *function_type_from_signature(
		const Signature &signature,
		llvm::LLVMContext &context)
	{
		auto input_types = types_of_type_infos(signature.input_extensions<LLVMTypeInfo>(), context);
		auto output_types = types_of_type_infos(signature.output_extensions<LLVMTypeInfo>(), context);
		llvm::Type *output_type = llvm::StructType::get(context, to_array_ref(output_types));
		return llvm::FunctionType::get(output_type, to_array_ref(input_types), false);
	}
};