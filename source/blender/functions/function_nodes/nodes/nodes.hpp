#include "nodecompiler/core.hpp"

namespace NC = LLVMNodeCompiler;

class AddIntegersNode : public NC::Node {
public:
	AddIntegersNode(uint amount, NC::Type *type);

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const;

private:
	uint amount;
	NC::Type *type;
};

class AddFloatsNode : public NC::Node {
public:
	AddFloatsNode(uint amount, NC::Type *type);

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const;

private:
	uint amount;
	NC::Type *type;
};

class Int32InputNode : public NC::Node {
public:
	Int32InputNode(int number);

	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &UNUSED(inputs),
		std::vector<llvm::Value *> &r_outputs) const;

private:
	int number;
};

class FloatInputNode : public NC::ExecuteFunctionNode {
public:
	FloatInputNode(float number);

private:
	static void execute(FloatInputNode *node, float *r_number);

	float number;
};