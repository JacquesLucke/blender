#pragma once

#include "FN_core.hpp"

namespace llvm {
	class ExecutionEngine;
	class Module;
	class LLVMContext;
	class FunctionType;
}

namespace FN {

	class CompiledLLVMBody : public FunctionBody {
	private:
		void *m_func_ptr;
		llvm::ExecutionEngine *m_engine;

		CompiledLLVMBody() = default;

	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		static CompiledLLVMBody *FromIR(
			llvm::Module *module,
			llvm::Function *main_func);

		~CompiledLLVMBody();

		void *function_ptr()
		{
			return m_func_ptr;
		}
	};

	bool try_ensure_CompiledLLVMBody(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */