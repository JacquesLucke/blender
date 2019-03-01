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
			llvm::LLVMContext &context);

		virtual llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value);

		virtual void build_free_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value);

	private:
		SmallMap<llvm::LLVMContext *, llvm::Type *> m_type_per_context;

		virtual llvm::Type *create_type(
			llvm::LLVMContext &context) = 0;
	};

	class SimpleLLVMTypeInfo : public LLVMTypeInfo {
	private:
		typedef std::function<llvm::Type *(llvm::LLVMContext &context)> CreateFunc;
		CreateFunc m_create_func;

		llvm::Type *create_type(llvm::LLVMContext &context)
		{
			return m_create_func(context);
		}

	public:
		SimpleLLVMTypeInfo(CreateFunc create_func)
			: m_create_func(create_func) {}
	};

} /* namespace FN */