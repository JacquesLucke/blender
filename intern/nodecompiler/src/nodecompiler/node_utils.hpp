#pragma once

#include "core_types.hpp"

namespace LLVMNodeCompiler {

class ExecuteFunctionNode : public Node {
public:
	void buildIR(
		llvm::IRBuilder<> &builder,
		std::vector<llvm::Value *> &inputs,
		std::vector<llvm::Value *> &r_outputs) const override;

	void set_execute_function(void *function, bool use_this);

private:
	const void *execute_function = nullptr;
	bool use_this = false;
};

} /* namespace LLVMNodeCompiler */