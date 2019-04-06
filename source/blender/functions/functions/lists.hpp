#pragma once

#include "FN_core.hpp"

namespace FN { namespace Functions {

	SharedType &get_list_type(SharedType &base_type);

	SharedFunction &empty_list(SharedType &base_type);
	SharedFunction &list_from_element(SharedType &base_type);
	SharedFunction &append_to_list(SharedType &base_type);
	SharedFunction &get_list_element(SharedType &base_type);
	SharedFunction &combine_lists(SharedType &base_type);
	SharedFunction &list_length(SharedType &base_type);

} } /* namespace FN::Functions */