#pragma once

#include "FN_core.hpp"

namespace llvm {
	class LLVMContext;
}

namespace FN {

	void derive_TupleCallBody_from_LLVMBuildIRBody(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */
