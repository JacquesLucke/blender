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


CombineVectorNode::CombineVectorNode()
{
	this->addInput("X", type_float);
	this->addInput("Y", type_float);
	this->addInput("Z", type_float);
	this->addOutput("Vector", type_vec3);
}

void CombineVectorNode::buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const
{
	llvm::Value *value = llvm::UndefValue::get(type_vec3->getLLVMType(builder.getContext()));
	value = builder.CreateInsertValue(value, inputs[0], 0);
	value = builder.CreateInsertValue(value, inputs[1], 1);
	value = builder.CreateInsertValue(value, inputs[2], 2);
	r_outputs.push_back(value);
}


SeparateVectorNode::SeparateVectorNode()
{
	this->addInput("Vector", type_vec3);
	this->addOutput("X", type_float);
	this->addOutput("Y", type_float);
	this->addOutput("Z", type_float);
}

void SeparateVectorNode::buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const
{
	r_outputs.push_back(builder.CreateExtractValue(inputs[0], 0));
	r_outputs.push_back(builder.CreateExtractValue(inputs[0], 1));
	r_outputs.push_back(builder.CreateExtractValue(inputs[0], 2));
}