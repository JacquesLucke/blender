#include "nodes.hpp"
#include "../types/types.hpp"

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


FloatInputNode::FloatInputNode(float number)
	: number(number)
{
	this->addOutput("Value", type_float);
}

void FloatInputNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &UNUSED(inputs),
	std::vector<llvm::Value *> &r_outputs) const
{
	r_outputs.push_back(llvm::ConstantFP::get(builder.getFloatTy(), this->number));
}


VectorInputNode::VectorInputNode(float x, float y, float z)
	: x(x), y(y), z(z)
{
	this->addOutput("Value", type_vec3);
}

void VectorInputNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &UNUSED(inputs),
	std::vector<llvm::Value *> &r_outputs) const
{
	llvm::Type *float_type = builder.getFloatTy();

	llvm::Value *value = llvm::UndefValue::get(type_vec3->getLLVMType(builder.getContext()));
	value = builder.CreateInsertValue(value, llvm::ConstantFP::get(float_type, this->x), (uint64_t)0);
	value = builder.CreateInsertValue(value, llvm::ConstantFP::get(float_type, this->y), (uint64_t)1);
	value = builder.CreateInsertValue(value, llvm::ConstantFP::get(float_type, this->z), (uint64_t)2);
	r_outputs.push_back(value);
}