#include "FN_llvm.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>


namespace FN {

	CompiledLLVM::~CompiledLLVM()
	{
		delete m_engine;
	}

	static void UNUSED_FUNCTION(optimize_module)(llvm::Module *module)
	{
		llvm::PassManagerBuilder builder;
		builder.OptLevel = 3;

		llvm::legacy::FunctionPassManager fpm(module);
		builder.populateFunctionPassManager(fpm);

		for (llvm::Function &function : module->functions()) {
			fpm.run(function);
		}

	}

	static void UNUSED_FUNCTION(save_machine_code)(
		std::string filepath,
		llvm::TargetMachine *target_machine,
		llvm::Module *module)
	{
		LLVMTargetMachineEmitToFile(
			(LLVMTargetMachineRef)target_machine,
			llvm::wrap(module),
			(char *)filepath.c_str(),
			LLVMAssemblyFile,
			NULL);
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