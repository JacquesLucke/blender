#include "builder.hpp"

namespace FN {

	LLVMTypes CodeBuilder::types_of_values(const LLVMValues &values)
	{
		LLVMTypes types;
		for (llvm::Value *value : values) {
			types.append(value->getType());
		}
		return types;
	}

} /* namespace FN */