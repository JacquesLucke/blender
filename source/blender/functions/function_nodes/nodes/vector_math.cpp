#include "nodes.hpp"
#include "../types/types.hpp"

AddVectorsNode::AddVectorsNode(uint amount)
	: amount(amount)
{
	assert(amount > 0);
	for (uint i = 0; i < amount; i++) {
		this->addInput("input " + std::to_string(i), type_vec3);
	}
	this->addOutput("result", type_vec3);
}

void AddVectorsNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	llvm::Value *result_x = llvm::ConstantFP::get(builder.getFloatTy(), 0);
	llvm::Value *result_y = result_x;
	llvm::Value *result_z = result_x;

	for (uint i = 0; i < this->amount; i++) {
		result_x = builder.CreateFAdd(result_x, builder.CreateExtractValue(inputs[i], 0));
		result_y = builder.CreateFAdd(result_y, builder.CreateExtractValue(inputs[i], 1));
		result_z = builder.CreateFAdd(result_z, builder.CreateExtractValue(inputs[i], 2));
	}

	llvm::Value *result = llvm::UndefValue::get(type_vec3->getLLVMType(builder.getContext()));
	result = builder.CreateInsertValue(result, result_x, 0);
	result = builder.CreateInsertValue(result, result_y, 1);
	result = builder.CreateInsertValue(result, result_z, 2);
	r_outputs.push_back(result);
}
