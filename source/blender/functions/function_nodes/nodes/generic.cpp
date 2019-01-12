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


SwitchNode::SwitchNode(NC::Type *type, uint amount)
	: amount(amount), type(type)
{
	this->addInput("Selector", type_int32);
	this->addInput("Default", this->type);
	for (uint i = 0; i < this->amount; i++) {
		this->addInput("Input " + std::to_string(i), this->type);
	}
	this->addOutput("Selected", this->type);
}

void SwitchNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	llvm::LLVMContext &context = builder.getContext();

	auto start_block = builder.GetInsertBlock();
	auto function = start_block->getParent();

	auto final_block = llvm::BasicBlock::Create(context, "Switch - Final", function);
	llvm::SwitchInst *switch_inst = builder.CreateSwitch(inputs[0], final_block, this->amount);

	llvm::IRBuilder<> final_builder(final_block);
	auto phi = final_builder.CreatePHI(this->type->getLLVMType(context), this->amount + 1);
	phi->addIncoming(inputs[1], start_block); /* default case */

	for (uint i = 0; i < this->amount; i++) {
		auto case_block = llvm::BasicBlock::Create(builder.getContext(), "Switch - Case " + std::to_string(i), function);
		switch_inst->addCase(builder.getInt32(i), case_block);
		llvm::IRBuilder<> case_builder(case_block);
		case_builder.CreateBr(final_block);
		phi->addIncoming(inputs[i + 2], case_block);
	}

	r_outputs.push_back(phi);
	builder.SetInsertPoint(final_block);
}