#pragma once

#include "llvm_gen.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

	class TupleCallBody;

	LLVMGenBody *llvm_body_for_tuple_call(
		TupleCallBody *tuple_call_body);

} /* namespace FN */