#pragma once

#include "FN_core.hpp"

namespace llvm {
class LLVMContext;
}

namespace FN {

void derive_LLVMBuildIRBody_from_TupleCallBody(Function &fn);

} /* namespace FN */
