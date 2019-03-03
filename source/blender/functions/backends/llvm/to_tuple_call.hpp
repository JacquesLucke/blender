#pragma once

#include "llvm_gen.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	class TupleCallBody;

	bool try_ensure_TupleCallBody(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */