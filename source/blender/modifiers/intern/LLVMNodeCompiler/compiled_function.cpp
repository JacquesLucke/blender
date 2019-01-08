#include "core.hpp"

namespace LLVMNodeCompiler {

CompiledFunction::CompiledFunction(void *function_pointer)
{
	this->function_pointer = function_pointer;
}

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
		input_types.push_back(socket.type()->getLLVMType(context));
	}

	std::vector<llvm::Type *> output_types;
	for (AnySocket socket : outputs) {
		output_types.push_back(socket.type()->getLLVMType(context));
	}

	llvm::StructType *return_type = llvm::StructType::create(output_types, name + " Output");

	llvm::FunctionType *function_type = llvm::FunctionType::get(
		return_type, input_types, false);

	llvm::Function *function = llvm::Function::Create(
		function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
		name, module);

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
	llvm::IRBuilder<> builder(context);
	builder.SetInsertPoint(bb);

	std::vector<llvm::Value *> input_values;
	for (uint i = 0; i < inputs.size(); i++) {
		input_values.push_back(function->arg_begin() + i);
	}

	std::vector<llvm::Value *> output_values;
	graph.generateCode(builder, inputs, outputs, input_values, output_values);

	llvm::Value *output = llvm::UndefValue::get(return_type);
	for (uint i = 0; i < outputs.size(); i++) {
		output = builder.CreateInsertValue(output, output_values[i], i);
	}
	builder.CreateRet(output);

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
	module->print(llvm::outs(), nullptr);
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
