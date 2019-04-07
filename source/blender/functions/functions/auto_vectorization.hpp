#pragma once

#include "FN_core.hpp"

namespace FN { namespace Functions {

	SharedFunction to_vectorized_function(
		SharedFunction &fn,
		const SmallVector<bool> &vectorize_input);

} } /* namespace FN::Functions */
