#pragma once

#include "core_types.hpp"

namespace LLVMNodeCompiler {

class ExecuteFunctionNode : public Node {
public:
	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const override;

	void setExecutionFunction(void *function, bool use_this);

private:
	const void *execute_function = nullptr;
	bool use_this = false;
};

/* For some reason I have to define these functions here to not get linker errors.
 * Usually they should be defined in node_utils.cpp.
 * Even though node_utils.cpp is compiled and linked, these functions aren't found.. */

inline void ExecuteFunctionNode::setExecutionFunction(void *function, bool use_this)
{
	this->execute_function = function;
	this->use_this = use_this;
}

inline void ExecuteFunctionNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	assert(this->execute_function);

	llvm::LLVMContext &context = builder.getContext();

	std::vector<llvm::Value *> arguments;
	if (this->use_this) {
		arguments.push_back(voidPtrToIR(builder, this));
	}

	for (auto input_value : inputs) {
		auto input_addr = builder.CreateAlloca(input_value->getType());
		builder.CreateStore(input_value, input_addr);
		arguments.push_back(input_addr);
	}

	std::vector<llvm::Value *> output_pointers;
	for (auto output_socket : this->outputs()) {
		auto output_type = output_socket.type->getLLVMType(context);
		auto output_addr = builder.CreateAlloca(output_type);
		output_pointers.push_back(output_addr);
		arguments.push_back(output_addr);
	}

	std::vector<llvm::Type *> argument_types;
	for (auto value : arguments) {
		argument_types.push_back(value->getType());
	}

	llvm::FunctionType *ftype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context), argument_types, false);
	callPointer(builder, this->execute_function, ftype, arguments);

	for (auto output_pointer : output_pointers) {
		llvm::Value *result = builder.CreateLoad(output_pointer);
		r_outputs.push_back(result);
	}
}


} /* namespace LLVMNodeCompiler */