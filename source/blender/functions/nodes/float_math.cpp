#include "nodes/nodes.hpp"

AddNumbersNode::AddNumbersNode(uint amount, NC::Type *type)
	: amount(amount), type(type)
{
	assert(amount > 0);

	for (uint i = 0; i < amount; i++) {
		this->addInput("Number", type);
	}

	this->addOutput("Result", type);
}

void AddNumbersNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	llvm::Value *output = inputs[0];
	for (uint i = 1; i < this->amount; i++) {
		output = builder.CreateAdd(output, inputs[i]);
	}
	r_outputs.push_back(output);
}