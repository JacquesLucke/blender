#pragma once

#include "core_types.hpp"

namespace LLVMNodeCompiler {

class ExecuteFunctionNode : public Node {
public:
	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs);

protected:
	void *execute_function = nullptr;
	bool use_this = false;
};

} /* namespace LLVMNodeCompiler */