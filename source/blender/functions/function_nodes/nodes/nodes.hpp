#include "nodecompiler/core.hpp"

namespace NC = LLVMNodeCompiler;

class AddNumbersNode : public NC::Node {
public:
	AddNumbersNode(uint amount, NC::Type *type);

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