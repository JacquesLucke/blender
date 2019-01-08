#include "core.hpp"

namespace LLVMNodeCompiler {

Link::Link(AnySocket from, AnySocket to)
	: from(from), to(to) {}

} /* namespace LLVMNodeCompiler */