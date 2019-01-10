#include "nodes.hpp"
#include "../types/types.hpp"


PassThroughNode::PassThroughNode(NC::Type *type)
{
	this->addInput("In", type);
	this->addOutput("Out", type);
}

void PassThroughNode::buildIR(
	llvm::IRBuilder<> &UNUSED(builder),
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	r_outputs.push_back(inputs[0]);
}