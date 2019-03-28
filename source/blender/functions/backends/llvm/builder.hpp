#pragma once

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	using LLVMValues = SmallVector<llvm::Value *>;
	using LLVMTypes = BLI::SmallVector<llvm::Type *>;
	class LLVMTypeInfo;

	template<typename T>
	static llvm::ArrayRef<T> to_array_ref(const SmallVector<T> &vector)
	{
		return llvm::ArrayRef<T>(vector.begin(), vector.end());
	}


	class CodeBuilder {
	private:
		llvm::IRBuilder<> m_builder;

	public:
		CodeBuilder(llvm::IRBuilder<> &builder)
			: m_builder(builder) {}

		CodeBuilder(llvm::BasicBlock *bb)
			: m_builder(bb) {}

		/* Getters
		 ***************************************/

		llvm::LLVMContext &getContext()
		{
			return m_builder.getContext();
		}

		llvm::Module *getModule()
		{
			return m_builder.GetInsertBlock()->getModule();
		}

		llvm::BasicBlock *GetInsertBlock()
		{
			return m_builder.GetInsertBlock();
		}

		llvm::Type *getVoidTy()
		{
			return m_builder.getVoidTy();
		}

		llvm::Type *getVoidPtrTy()
		{
			return this->getVoidTy()->getPointerTo();
		}

		llvm::Type *getVoidPtrPtrTy()
		{
			return this->getVoidPtrTy()->getPointerTo();
		}

		llvm::Type *getFixedSizeType(uint size)
		{
			return llvm::ArrayType::get(m_builder.getInt8Ty(), size);
		}


		/* Value Builders
		 **************************************/

		llvm::Value *getVoidPtr(void *ptr)
		{
			return this->getPtr(ptr, this->getVoidPtrTy());
		}

		llvm::Value *getPtr(void *ptr, llvm::Type *ptr_type)
		{
			llvm::Value *ptr_value = m_builder.getInt64((size_t)ptr);
			return m_builder.CreateIntToPtr(ptr_value, ptr_type);
		}

		llvm::Value *getInt64(int64_t value)
		{
			return m_builder.getInt64(value);
		}


		/* Misc
		 **************************************/

		LLVMTypes types_of_values(const LLVMValues &values);


		/* Instruction Builders
		 **************************************/

		void CreateRet(llvm::Value *value)
		{
			m_builder.CreateRet(value);
		}

		void CreateRetVoid()
		{
			m_builder.CreateRetVoid();
		}

		llvm::Value *CreateCastIntTo8(llvm::Value *value, bool is_signed)
		{
			return m_builder.CreateIntCast(value, m_builder.getInt8Ty(), is_signed);
		}

		llvm::Value *CreateCastIntTo1(llvm::Value *value)
		{
			return m_builder.CreateIntCast(value, m_builder.getInt1Ty(), false);
		}

		llvm::Value *CreateFAdd(llvm::Value *a, llvm::Value *b)
		{
			return m_builder.CreateFAdd(a, b);
		}

		llvm::Value *CreateAllocaBytes_VoidPtr(uint amount)
		{
			llvm::Type *size_type = this->getFixedSizeType(amount);
			llvm::Value *addr = m_builder.CreateAlloca(size_type);
			return this->CastToVoidPtr(addr);
		}

		llvm::Value *CreateAllocaBytes_BytePtr(uint amount)
		{
			llvm::Type *size_type = this->getFixedSizeType(amount);
			llvm::Value *addr = m_builder.CreateAlloca(size_type);
			return this->CastToBytePtr(addr);
		}

		llvm::Value *CastToPointerOf(llvm::Value *addr, llvm::Type *base_type)
		{
			return m_builder.CreatePointerCast(addr, base_type->getPointerTo());
		}

		llvm::Value *CastToVoidPtr(llvm::Value *addr)
		{
			return m_builder.CreatePointerCast(addr, this->getVoidPtrTy());
		}

		llvm::Value *CastToBytePtr(llvm::Value *addr)
		{
			return m_builder.CreatePointerCast(addr, m_builder.getInt8PtrTy());
		}

		llvm::Value *CreateLoad(llvm::Value *addr)
		{
			return m_builder.CreateLoad(addr);
		}

		void CreateStore(llvm::Value *value, llvm::Value *addr)
		{
			m_builder.CreateStore(value, addr, false);
		}

		llvm::Value *CreateExtractValue(llvm::Value *aggregate, uint index)
		{
			return m_builder.CreateExtractValue(aggregate, index);
		}

		llvm::Value *CreateInsertValue(llvm::Value *aggregate, llvm::Value *value, uint index)
		{
			return m_builder.CreateInsertValue(aggregate, value, index);
		}

		llvm::Value *CreateCallPointer(
			void *func_ptr,
			llvm::FunctionType *ftype,
			const LLVMValues &args)
		{
			auto address_int = m_builder.getInt64((size_t)func_ptr);
			auto address = m_builder.CreateIntToPtr(address_int, ftype->getPointerTo());
			return m_builder.CreateCall(address, to_array_ref(args));
		}

		llvm::Value *CreateCall(
			llvm::Function *function,
			const LLVMValues &args)
		{
			return m_builder.CreateCall(function, to_array_ref(args));
		}

		llvm::Value *CreateConstGEP1_32(llvm::Value *addr, uint index)
		{
			return m_builder.CreateConstGEP1_32(addr, index);
		}

		llvm::Value *CreateGEP(llvm::Value *addr, llvm::Value *index)
		{
			return m_builder.CreateGEP(addr, index);
		}
	};

} /* namespace FN */