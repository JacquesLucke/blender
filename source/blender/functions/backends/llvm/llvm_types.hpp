#pragma once

#include "FN_core.hpp"

#include "llvm/IR/IRBuilder.h"

#include <functional>
#include <mutex>

namespace FN {

	class LLVMTypeInfo {
	public:
		BLI_COMPOSITION_DECLARATION(LLVMTypeInfo);

		virtual ~LLVMTypeInfo() {}

		llvm::Type *get_type(
			llvm::LLVMContext &context) const;

		llvm::Type *get_type_ptr(
			llvm::LLVMContext &context) const;

		virtual llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const = 0;

		virtual void build_free_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const = 0;

		virtual void build_store_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const = 0;

		virtual llvm::Value *build_load_ir__copy(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const = 0;

		virtual llvm::Value *build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const = 0;

	private:
		mutable SmallMap<llvm::LLVMContext *, llvm::Type *> m_type_per_context;
		mutable std::mutex m_type_per_context_mutex;

		void ensure_type_exists_for_context(llvm::LLVMContext &context) const;

		virtual llvm::Type *create_type(
			llvm::LLVMContext &context) const = 0;
	};

	class SimpleLLVMTypeInfo : public LLVMTypeInfo {
	private:
		typedef std::function<llvm::Type *(llvm::LLVMContext &context)> CreateFunc;
		CreateFunc m_create_func;

		llvm::Type *create_type(llvm::LLVMContext &context) const override
		{
			return m_create_func(context);
		}

	public:
		SimpleLLVMTypeInfo(CreateFunc create_func)
			: m_create_func(create_func) {}

		llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const override;

		void build_free_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const override;

		void build_store_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__copy(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override;
	};

	class PointerLLVMTypeInfo : public LLVMTypeInfo {
	private:
		typedef std::function<void *(void *)> CopyFunc;
		typedef std::function<void (void *)> FreeFunc;
		typedef std::function<void *()> DefaultFunc;

		CopyFunc m_copy_func;
		FreeFunc m_free_func;
		DefaultFunc m_default_func;

		llvm::Type *create_type(llvm::LLVMContext &context) const override
		{
			return llvm::Type::getVoidTy(context)->getPointerTo();
		}

		static void *copy_value(PointerLLVMTypeInfo *info, void *value);
		static void free_value(PointerLLVMTypeInfo *info, void *value);
		static void *default_value(PointerLLVMTypeInfo *info);

	public:
		PointerLLVMTypeInfo(CopyFunc copy_func, FreeFunc free_func, DefaultFunc default_func)
			: m_copy_func(copy_func),
			  m_free_func(free_func),
			  m_default_func(default_func) {}

		llvm::Value *build_copy_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const override;

		void build_free_ir(
			llvm::IRBuilder<> &builder,
			llvm::Value *value) const override;

		void build_store_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__copy(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__relocate(
			llvm::IRBuilder<> &builder,
			llvm::Value *byte_addr) const override;
	};

	inline LLVMTypeInfo *get_type_info(const SharedType &type)
	{
		auto ext = type->extension<LLVMTypeInfo>();
		BLI_assert(ext);
		return ext;
	}

	inline llvm::Type *get_llvm_type(SharedType &type, llvm::LLVMContext &context)
	{
		return get_type_info(type)->get_type(context);
	}

} /* namespace FN */