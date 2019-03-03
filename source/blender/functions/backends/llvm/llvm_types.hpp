#pragma once

#include "FN_core.hpp"

#include "llvm/IR/IRBuilder.h"

#include <functional>

namespace FN {

	class LLVMTypeInfo {
	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		virtual ~LLVMTypeInfo() {}

		llvm::Type *get_type(
			llvm::LLVMContext &context) const;

		llvm::Type *get_type_ptr(
			llvm::LLVMContext &context) const;

		virtual llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const;

		virtual void build_free_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const;

		virtual void build_store_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const;

		virtual llvm::Value *build_load_ir__copy(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const;

		virtual llvm::Value *build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const;

	private:
		mutable SmallMap<llvm::LLVMContext *, llvm::Type *> m_type_per_context;

		virtual llvm::Type *create_type(
			llvm::LLVMContext &context) const = 0;
	};

	class SimpleLLVMTypeInfo : public LLVMTypeInfo {
	private:
		typedef std::function<llvm::Type *(llvm::LLVMContext &context)> CreateFunc;
		CreateFunc m_create_func;

		llvm::Type *create_type(llvm::LLVMContext &context) const
		{
			return m_create_func(context);
		}

	public:
		SimpleLLVMTypeInfo(CreateFunc create_func)
			: m_create_func(create_func) {}
	};

	inline LLVMTypeInfo *get_type_info(const SharedType &type)
	{
		auto ext = type->extension<LLVMTypeInfo>();
		BLI_assert(ext);
		return ext;
	}

} /* namespace FN */