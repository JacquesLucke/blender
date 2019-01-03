#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

extern "C" {
	void test_llvm(void);
}

void test_llvm() {
	llvm::LLVMContext *context = new llvm::LLVMContext();
	llvm::Module *module = new llvm::Module("test", *context);

	std::vector<llvm::Type *> arg_types = {
		llvm::Type::getInt32Ty(*context),
		llvm::Type::getInt32Ty(*context)};

	llvm::FunctionType *ftype = llvm::FunctionType::get(
		llvm::Type::getInt32Ty(*context), arg_types, false);

	llvm::Function *func = llvm::Function::Create(
		ftype, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "my_func", module);

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(*context, "entry", func);

	llvm::IRBuilder<> builder(*context);
	builder.SetInsertPoint(bb);

	llvm::Argument *arg0 = func->arg_begin() + 0;
	llvm::Argument *arg1 = func->arg_begin() + 1;

	llvm::Value *result = builder.CreateAdd(arg0, arg1, "result");
	builder.CreateRet(result);

	llvm::verifyFunction(*func, &llvm::outs());
	llvm::verifyModule(*module, &llvm::outs());

	module->print(llvm::outs(), nullptr);

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module)).create();
	ee->finalizeObject();

	int (*add)(int, int) = (int (*)(int, int))ee->getFunctionAddress("my_func");
	printf("Pointer: %p\n", add);
	printf("Result: %d\n", add(43, 10));
}

