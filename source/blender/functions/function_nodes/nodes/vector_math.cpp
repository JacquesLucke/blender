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
	this->setExecutionFunction((void *)this->execute, false);
}

void CombineVectorNode::execute(float *x, float *y, float *z, Vector3 *r_vector)
{
	*r_vector = {*x, *y, *z};
}


SeparateVectorNode::SeparateVectorNode()
{
	this->addInput("Vector", type_vec3);
	this->addOutput("X", type_float);
	this->addOutput("Y", type_float);
	this->addOutput("Z", type_float);
	this->setExecutionFunction((void *)this->execute, false);
}

void SeparateVectorNode::execute(Vector3 *vector, float *r_x, float *r_y, float *r_z)
{
	std::cout << "Vector: " << vector->x << " " << vector->y << " " << vector->z << std::endl;
	*r_x = vector->x;
	*r_y = vector->y;
	*r_z = vector->z;
}