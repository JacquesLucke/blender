#include "llvm_gen.hpp"

namespace FN {

	const char *LLVMGenerateIRBody::identifier_in_composition()
	{
		return "LLVM Gen Body";
	}

	void LLVMGenerateIRBody::free_self(void *value)
	{
		LLVMGenerateIRBody *v = (LLVMGenerateIRBody *)value;
		delete v;
	}

} /* namespace FN */