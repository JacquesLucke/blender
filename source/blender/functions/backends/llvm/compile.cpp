#include "compile.hpp"
#include "BLI_utildefines.h"

#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace FN {

	CompiledLLVM::~CompiledLLVM()
	{
		delete m_engine;
	}

	std::unique_ptr<CompiledLLVM>
	CompiledLLVM::FromIR(
		llvm::Module *module,
		llvm::Function *main_function)
	{
		BLI_assert(!llvm::verifyModule(*module, &llvm::outs()));
		// module->print(llvm::outs(), nullptr);

		llvm::ExecutionEngine *ee = llvm::EngineBuilder(
			std::unique_ptr<llvm::Module>(module)).create();
		ee->finalizeObject();
		ee->generateCodeForModule(module);


		uint64_t function_ptr = ee->getFunctionAddress(
			main_function->getName().str());

		auto result = std::unique_ptr<CompiledLLVM>(new CompiledLLVM());
		result->m_engine = ee;
		result->m_func_ptr = (void *)function_ptr;
		return result;
	}

} /* namespace FN */