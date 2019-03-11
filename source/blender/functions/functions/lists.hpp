#pragma once

#include "FN_core.hpp"

namespace FN { namespace Functions {

	SharedFunction &append_float();
	SharedFunction &get_float_list_element();
	SharedFunction &combine_float_lists();

	SharedFunction &append_fvec3();
	SharedFunction &get_fvec3_list_element();
	SharedFunction &combine_fvec3_lists();

} } /* namespace FN::Functions */