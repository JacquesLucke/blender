#include "nodes.hpp"
#include "../types/types.hpp"

FloatToIntNode::FloatToIntNode()
{
    this->addInput("In", type_float);
	this->addOutput("Out", type_int32);
}

void FloatToIntNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
    r_outputs.push_back(builder.CreateCast(llvm::Instruction::CastOps::FPToSI, inputs[0], type_int32->getLLVMType(builder.getContext())));
}