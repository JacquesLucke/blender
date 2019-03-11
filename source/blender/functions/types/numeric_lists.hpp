#pragma once

#include "FN_core.hpp"
#include "lists.hpp"
#include "numeric.hpp"

namespace FN { namespace Types {

	using SharedFloatList = SharedList<float>;
	using SharedFVec3List = SharedList<Vector>;

	SharedType &get_float_list_type();
	SharedType &get_fvec3_list_type();

} } /* namespace FN::Types */