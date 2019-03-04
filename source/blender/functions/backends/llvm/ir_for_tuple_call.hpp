#pragma once

#include "FN_core.hpp"

namespace llvm {
	class LLVMContext;
}

namespace FN {

	void derive_LLVMBuildIRBody_from_TupleCallBody(
		SharedFunction &fn);

} /* namespace FN */