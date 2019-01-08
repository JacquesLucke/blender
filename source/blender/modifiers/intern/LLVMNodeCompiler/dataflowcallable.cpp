#include "core.hpp"

namespace LLVMNodeCompiler {

DataFlowCallable::DataFlowCallable(
	llvm::Module *module,
	llvm::ExecutionEngine *ee,
	std::string function_name)
	: module(module), ee(ee)
{
	this->function_pointer = (void *)this->ee->getFunctionAddress(function_name);
}

void *DataFlowCallable::getFunctionPointer()
{
	return this->function_pointer;
}

void DataFlowCallable::printCode()
{
	this->module->print(llvm::outs(), nullptr);
}

} /* namespace LLVMNodeCompiler */
