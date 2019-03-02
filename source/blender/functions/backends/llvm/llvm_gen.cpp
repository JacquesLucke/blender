#include "llvm_gen.hpp"

namespace FN {

	const char *LLVMGenBody::identifier_in_composition()
	{
		return "LLVM Gen Body";
	}

	void LLVMGenBody::free_self(void *value)
	{
		LLVMGenBody *v = (LLVMGenBody *)value;
		delete v;
	}

} /* namespace FN */