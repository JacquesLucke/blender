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

	class CompiledLLVMBody : public FunctionBody {
	private:
		std::unique_ptr<CompiledLLVM> m_compiled;

		CompiledLLVMBody() = default;

	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		CompiledLLVMBody(std::unique_ptr<CompiledLLVM> compiled)
			: m_compiled(std::move(compiled)) {}

		void *function_ptr()
		{
			return m_compiled->function_ptr();
		}
	};

	bool try_ensure_CompiledLLVMBody(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */