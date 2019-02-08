#include "cpu.hpp"

namespace FN {

	const char *TupleCallBody::identifier_in_composition()
	{
		return "Tuple Call Body";
	}

	void TupleCallBody::free_self(void *value)
	{
		TupleCallBody *v = (TupleCallBody *)value;
		delete v;
	}


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