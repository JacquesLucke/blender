#pragma once

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	using LLVMValues = SmallVector<llvm::Value *>;
	using LLVMTypes = BLI::SmallVector<llvm::Type *>;

	class LLVMGenBody : public FunctionBody {
	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		virtual ~LLVMGenBody() {};

		virtual void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const = 0;
	};

}