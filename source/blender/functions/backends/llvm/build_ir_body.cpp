#include "build_ir_body.hpp"

namespace FN {

	const char *LLVMBuildIRBody::identifier_in_composition()
	{
		return "LLVM Gen Body";
	}

	void LLVMBuildIRBody::free_self(void *value)
	{
		LLVMBuildIRBody *v = (LLVMBuildIRBody *)value;
		delete v;
	}

} /* namespace FN */