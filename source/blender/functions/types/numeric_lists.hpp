#pragma once

#include "FN_core.hpp"
#include "lists.hpp"

namespace FN { namespace Types {

	using SharedFloatList = SharedList<float>;

	SharedType &get_float_list_type();

} } /* namespace FN::Types */