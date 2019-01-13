#include "nodes.hpp"
#include "../types/types.hpp"

static llvm::Value *convert_VectorToIRVector(llvm::IRBuilder<> &builder, llvm::Value *vector)
{
	llvm::Type *vector_type = llvm::VectorType::get(builder.getFloatTy(), 3);
	llvm::Value *output = llvm::UndefValue::get(vector_type);
	for (uint64_t i = 0; i < 3; i++) {
		output = builder.CreateInsertElement(output, builder.CreateExtractValue(vector, i), i);
	}
	return output;
}

static llvm::Value *convert_IRVectorToVector(llvm::IRBuilder<> &builder, llvm::Value *vector)
{
	llvm::Value *output = llvm::UndefValue::get(type_vec3->getLLVMType(builder.getContext()));
	for (uint64_t i = 0; i < 3; i++) {
		output = builder.CreateInsertValue(output, builder.CreateExtractElement(vector, i), i);
	}
	return output;
}

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
	llvm::Value *result = convert_VectorToIRVector(builder, inputs[0]);
	for (uint i = 1; i < this->amount; i++) {
		result = builder.CreateFAdd(result, convert_VectorToIRVector(builder, inputs[i]));
	}
	r_outputs.push_back(convert_IRVectorToVector(builder, result));
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