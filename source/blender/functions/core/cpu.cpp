#include "cpu.hpp"

namespace FN {

	const char *TupleCallBody::identifier()
	{
		return "Tuple Call Body";
	}

	void TupleCallBody::free(void *value)
	{
		TupleCallBody *v = (TupleCallBody *)value;
		delete v;
	}


	const char *CPPTypeInfo::identifier()
	{
		return "C++ Type Info";
	}

	void CPPTypeInfo::free(void *value)
	{
		CPPTypeInfo *value_ = (CPPTypeInfo *)value;
		delete value_;
	}

} /* namespace FN */