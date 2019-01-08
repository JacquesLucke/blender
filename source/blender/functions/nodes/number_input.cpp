#include "nodes/nodes.hpp"
#include "types/types.hpp"

Int32InputNode::Int32InputNode(int number)
	: number(number)
{
	this->addOutput("Value", type_int32);
}

void Int32InputNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &UNUSED(inputs),
	std::vector<llvm::Value *> &r_outputs) const
{
	r_outputs.push_back(builder.getInt32(this->number));
}
