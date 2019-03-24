#pragma once

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	using LLVMValues = SmallVector<llvm::Value *>;
	using LLVMTypes = BLI::SmallVector<llvm::Type *>;

	class LLVMBuildIRBody : public FunctionBody {
	public:
		BLI_COMPOSITION_DECLARATION(LLVMBuildIRBody);

		virtual ~LLVMBuildIRBody() {};

		virtual void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const = 0;
	};

}