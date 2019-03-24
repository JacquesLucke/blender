#pragma once

#include "FN_core.hpp"
#include "compile.hpp"

namespace llvm {
	class ExecutionEngine;
	class Module;
	class LLVMContext;
	class FunctionType;
}

namespace FN {

	class LLVMCompiledBody : public FunctionBody {
	private:
		std::unique_ptr<CompiledLLVM> m_compiled;

		LLVMCompiledBody() = default;

	public:
		BLI_COMPOSITION_DECLARATION(LLVMCompiledBody);

		LLVMCompiledBody(std::unique_ptr<CompiledLLVM> compiled)
			: m_compiled(std::move(compiled)) {}

		void *function_ptr() const
		{
			return m_compiled->function_ptr();
		}

		void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const;
	};

	void derive_LLVMCompiledBody_from_LLVMBuildIRBody(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */