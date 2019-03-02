#pragma once

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	class TupleCallBody;

	TupleCallBody *compile_llvm_to_tuple_call(
		SharedFunction &fn,
		llvm::LLVMContext &context);

} /* namespace FN */