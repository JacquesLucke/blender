#pragma once

#include "FN_core.hpp"

namespace FN { namespace Functions {

	SharedFunction auto_vectorization(
		SharedFunction &fn,
		const SmallVector<bool> &vectorize_input);

} } /* namespace FN::Functions */