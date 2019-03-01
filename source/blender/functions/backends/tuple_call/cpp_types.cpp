#include "cpp_types.hpp"

namespace FN {

	const char *CPPTypeInfo::identifier_in_composition()
	{
		return "C++ Type Info";
	}

	void CPPTypeInfo::free_self(void *value)
	{
		CPPTypeInfo *value_ = (CPPTypeInfo *)value;
		delete value_;
	}

} /* namespace FN */