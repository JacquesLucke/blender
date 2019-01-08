#include "core.hpp"

namespace LLVMNodeCompiler {

SocketInfo::SocketInfo(std::string debug_name, Type *type)
	: debug_name(debug_name), type(type) {}

} /* namespace LLVMNodeCompiler */