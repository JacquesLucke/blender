#include "core.hpp"

namespace LLVMNodeCompiler {

CompiledFunction::CompiledFunction(void *function_pointer)
{
	this->function_pointer = function_pointer;
}

CompiledFunction::~CompiledFunction() {}

void *CompiledFunction::pointer()
{
	return this->function_pointer;
}

CompiledLLVMFunction::CompiledLLVMFunction(
	llvm::ExecutionEngine *ee,
	llvm::Module *module,
	std::string function_name)
	: CompiledFunction((void *)ee->getFunctionAddress(function_name)),
	  ee(ee), module(module) {}

CompiledLLVMFunction::~CompiledLLVMFunction()
{
	delete ee;
}

void CompiledLLVMFunction::printCode()
{
	this->module->print(llvm::outs(), nullptr);
}

static llvm::Function *generateFunction(
	llvm::Module *module, std::string name,
	DataFlowGraph &graph, SocketArraySet &inputs, SocketArraySet &outputs)
{
	llvm::LLVMContext &context = module->getContext();

	std::vector<llvm::Type *> input_types;
	for (AnySocket socket : inputs) {
		input_types.push_back(socket.type()->getLLVMType(context)->getPointerTo());
	}
	for (AnySocket socket : outputs) {
		input_types.push_back(socket.type()->getLLVMType(context)->getPointerTo());
	}

	llvm::FunctionType *function_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context), input_types, false);

	llvm::Function *function = llvm::Function::Create(
		function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
		name, module);

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
	llvm::IRBuilder<> builder(context);
	builder.SetInsertPoint(bb);

	std::vector<llvm::Value *> input_values;
	for (uint i = 0; i < inputs.size(); i++) {
		// llvm::Value *value = llvm::UndefValue::get(inputs[i].type()->getLLVMType(context));
		llvm::Value *value = builder.CreateLoad(function->arg_begin() + i);
		input_values.push_back(value);
	}

	std::vector<llvm::Value *> output_values;
	graph.generateCode(builder, inputs, outputs, input_values, output_values);

	for (uint i = 0; i < outputs.size(); i++) {
		llvm::Value *target_addr = function->arg_begin() + inputs.size() + i;
		builder.CreateStore(output_values[i], target_addr);
	}
	builder.CreateRetVoid();

	llvm::verifyFunction(*function, &llvm::outs());
	llvm::verifyModule(*module, &llvm::outs());

	return function;
}

static llvm::Module *generateModule(
	llvm::LLVMContext &context,
	std::string module_name, std::string function_name,
	DataFlowGraph &graph, SocketArraySet &inputs, SocketArraySet &outputs)
{
	assert(outputs.size() > 0);
	llvm::Module *module = new llvm::Module(module_name, context);
	generateFunction(module, function_name, graph, inputs, outputs);
	return module;
}

CompiledLLVMFunction *compileDataFlow(
	DataFlowGraph &graph,
	SocketArraySet &inputs,
	SocketArraySet &outputs)
{
	llvm::LLVMContext *context = new llvm::LLVMContext();

	std::string debug_name = "Test";
	std::string module_name = debug_name + " Module";
	std::string function_name = debug_name + " Function";

	llvm::Module *module = generateModule(
		*context,
		module_name, function_name,
		graph, inputs, outputs);

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module)).create();
	ee->finalizeObject();
	ee->generateCodeForModule(module);

	return new CompiledLLVMFunction(ee, module, function_name);
}

} /* namespace LLVMNodeCompiler */
