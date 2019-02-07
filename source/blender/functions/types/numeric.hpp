#pragma once

#include "../FN_functions.hpp"

namespace FN::Types {

	void init_numeric_types();

	SharedType &get_float_type();
	SharedType &get_int32_type();
	SharedType &get_fvec3_type();

} /* namespace FN::Types */