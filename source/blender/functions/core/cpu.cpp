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


	const char *TypeSize::identifier()
	{
		return "Type Size";
	}

	void TypeSize::free(void *value)
	{
		TypeSize *v = (TypeSize *)value;
		delete v;
	}

} /* namespace FN */