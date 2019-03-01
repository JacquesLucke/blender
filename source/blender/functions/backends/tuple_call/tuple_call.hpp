#pragma once

#include "tuple.hpp"

namespace FN {

	class TupleCallBody {
	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		virtual ~TupleCallBody() {};

		virtual void call(const Tuple &fn_in, Tuple &fn_out) const = 0;
		virtual void dependencies(Dependencies &UNUSED(deps)) const {}
		virtual void init_defaults(Tuple &fn_in) const;
	};

} /* namespace FN */