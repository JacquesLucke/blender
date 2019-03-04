#pragma once

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	using LLVMValues = SmallVector<llvm::Value *>;
	using LLVMTypes = BLI::SmallVector<llvm::Type *>;

	class LLVMBuildIRBody : public FunctionBody {
	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		virtual ~LLVMBuildIRBody() {};

		virtual void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const = 0;
	};

}