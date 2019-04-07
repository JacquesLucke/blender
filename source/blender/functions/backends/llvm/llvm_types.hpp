#pragma once

#include "FN_core.hpp"

#include "builder.hpp"

#include <functional>
#include <mutex>

namespace FN {

	class LLVMTypeInfo : public TypeExtension {
	public:
		BLI_COMPOSITION_DECLARATION(LLVMTypeInfo);

		virtual ~LLVMTypeInfo() {}

		virtual llvm::Type *get_type(
			llvm::LLVMContext &context) const = 0;

		virtual llvm::Value *build_copy_ir(
			CodeBuilder &builder,
			llvm::Value *value) const = 0;

		virtual void build_free_ir(
			CodeBuilder &builder,
			llvm::Value *value) const = 0;

		virtual void build_store_ir__relocate(
			CodeBuilder &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const = 0;

		virtual llvm::Value *build_load_ir__copy(
			CodeBuilder &builder,
			llvm::Value *byte_addr) const = 0;

		virtual llvm::Value *build_load_ir__relocate(
			CodeBuilder &builder,
			llvm::Value *byte_addr) const = 0;
	};

	class SimpleLLVMTypeInfo : public LLVMTypeInfo {
	private:
		typedef std::function<llvm::Type *(llvm::LLVMContext &context)> CreateFunc;
		CreateFunc m_create_func;

	public:
		SimpleLLVMTypeInfo(CreateFunc create_func)
			: m_create_func(create_func) {}

		llvm::Type *get_type(llvm::LLVMContext &context) const override
		{
			return m_create_func(context);
		}

		llvm::Value *build_copy_ir(
			CodeBuilder &builder,
			llvm::Value *value) const override;

		void build_free_ir(
			CodeBuilder &builder,
			llvm::Value *value) const override;

		void build_store_ir__relocate(
			CodeBuilder &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__copy(
			CodeBuilder &builder,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__relocate(
			CodeBuilder &builder,
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

		static void *copy_value(PointerLLVMTypeInfo *info, void *value);
		static void free_value(PointerLLVMTypeInfo *info, void *value);
		static void *default_value(PointerLLVMTypeInfo *info);

	public:
		PointerLLVMTypeInfo(CopyFunc copy_func, FreeFunc free_func, DefaultFunc default_func)
			: m_copy_func(copy_func),
			  m_free_func(free_func),
			  m_default_func(default_func) {}

		llvm::Type *get_type(llvm::LLVMContext &context) const override
		{
			return llvm::Type::getVoidTy(context)->getPointerTo();
		}

		llvm::Value *build_copy_ir(
			CodeBuilder &builder,
			llvm::Value *value) const override;

		void build_free_ir(
			CodeBuilder &builder,
			llvm::Value *value) const override;

		void build_store_ir__relocate(
			CodeBuilder &builder,
			llvm::Value *value,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__copy(
			CodeBuilder &builder,
			llvm::Value *byte_addr) const override;

		llvm::Value *build_load_ir__relocate(
			CodeBuilder &builder,
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

	LLVMTypes types_of_type_infos(
		const SmallVector<LLVMTypeInfo *> &type_infos,
		llvm::LLVMContext &context);

	llvm::FunctionType *function_type_from_signature(
		const Signature &signature,
		llvm::LLVMContext &context);

} /* namespace FN */
