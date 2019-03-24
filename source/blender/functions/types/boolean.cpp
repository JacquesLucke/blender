#include "boolean.hpp"

#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN { namespace Types {

	class LLVMBool : public LLVMTypeInfo {

		llvm::Type *create_type(
			llvm::LLVMContext &context) const override
		{
			return llvm::Type::getInt1Ty(context);
		}

		llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &UNUSED(builder),
			llvm::Value *value) const override
		{
			return value;
		}

		void build_free_ir(
			llvm::IRBuilder<> &UNUSED(builder),
			llvm::Value *UNUSED(value)) const override
		{
			return;
		}

		void build_store_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const override
		{
			llvm::Value *byte_value = builder.CreateIntCast(
				value, builder.getInt8Ty(), false);
			builder.CreateStore(byte_value, byte_addr, false);
		}

		llvm::Value *build_load_ir__copy(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override
		{
			llvm::Value *byte_value = builder.CreateLoad(byte_addr);
			llvm::Value *value = builder.CreateIntCast(
				byte_value, builder.getInt1Ty(), false);
			return value;
		}

		llvm::Value *build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override
		{
			return this->build_load_ir__copy(builder, byte_addr);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedType, get_bool_type)
	{
		SharedType type = SharedType::New("Bool");
		type->extend(new CPPTypeInfoForType<bool>());
		type->extend(new LLVMBool());
		return type;
	}

} } /* namespace FN::Types */