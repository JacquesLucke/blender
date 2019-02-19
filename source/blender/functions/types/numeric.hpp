#pragma once

#include "../FN_functions.hpp"

namespace FN { namespace Types {

	struct Vector {
		float x, y, z;
	};

	SharedType &get_float_type();
	SharedType &get_int32_type();
	SharedType &get_fvec3_type();
	SharedType &get_float_list_type();

} } /* namespace FN::Types */