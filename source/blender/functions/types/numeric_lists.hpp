#pragma once

#include "FN_core.hpp"
#include "lists.hpp"
#include "numeric.hpp"

namespace FN { namespace Types {

	using SharedFloatList = SharedList<float>;
	using SharedFVec3List = SharedList<Vector>;
	using SharedInt32List = SharedList<int32_t>;

	SharedType &GET_TYPE_float_list();
	SharedType &GET_TYPE_fvec3_list();
	SharedType &GET_TYPE_int32_list();

} } /* namespace FN::Types */