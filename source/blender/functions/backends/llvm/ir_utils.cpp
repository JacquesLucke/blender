#include "ir_utils.hpp"

namespace FN {

	llvm::CallInst *call_pointer(
		llvm::IRBuilder<> &builder,
		const void *pointer,
		llvm::FunctionType *type,
		LLVMValues arguments)
	{
		auto address_int = builder.getInt64((size_t)pointer);
		auto address = builder.CreateIntToPtr(address_int, type->getPointerTo());
		return builder.CreateCall(address, to_array_ref(arguments));
	}

	llvm::Value *lookup_tuple_address(
		llvm::IRBuilder<> &builder,
		llvm::Value *data_addr,
		llvm::Value *offsets_addr,
		uint index)
	{
		llvm::Value *offset_addr = builder.CreateConstGEP1_32(offsets_addr, index);
		llvm::Value *offset = builder.CreateLoad(offset_addr);
		llvm::Value *value_byte_addr = builder.CreateGEP(data_addr, offset);
		return value_byte_addr;
	}

	llvm::Value *void_ptr_to_ir(llvm::IRBuilder<> &builder, void *ptr)
	{
		return ptr_to_ir(builder, ptr, builder.getVoidTy()->getPointerTo());
	}

	llvm::Value *int_ptr_to_ir(llvm::IRBuilder<> &builder, int *ptr)
	{
		return ptr_to_ir(builder, ptr, builder.getInt32Ty()->getPointerTo());
	}

	llvm::Value *byte_ptr_to_ir(llvm::IRBuilder<> &builder, void *ptr)
	{
		return ptr_to_ir(builder, ptr, builder.getInt8PtrTy());
	}

	llvm::Value *ptr_to_ir(llvm::IRBuilder<> &builder, void *ptr, llvm::Type *type)
	{
		return builder.CreateIntToPtr(builder.getInt64((size_t)ptr), type);
	}

	llvm::Value *alloca_bytes(llvm::IRBuilder<> &builder, uint size)
	{
		llvm::Type *size_type = llvm::ArrayType::get(
				builder.getInt8Ty(), size);

		llvm::Value *tuple_in_ptr = builder.CreateAlloca(size_type);
		return builder.CreatePointerCast(tuple_in_ptr, builder.getInt8PtrTy());
	}

	LLVMTypes types_of_values(const LLVMValues &values)
	{
		LLVMTypes types;
		for (llvm::Value *value : values) {
			types.append(value->getType());
		}
		return types;
	}

} /* namespace FN */