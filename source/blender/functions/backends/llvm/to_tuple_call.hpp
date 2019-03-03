#pragma once

#include "llvm_gen.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	class TupleCallBody;

	TupleCallBody *compile_llvm_to_tuple_call(
		LLVMGenBody *llvm_body,
		llvm::LLVMContext &context);

} /* namespace FN */