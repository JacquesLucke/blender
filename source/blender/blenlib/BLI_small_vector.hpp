#pragma once

#include "BLI_utildefines.h"
#include <vector>

namespace BLI {

	template<typename T, uint N = 4>
	class SmallVector {
	public:
		SmallVector() {}

	private:
		std::vector<T> elements;
	};

} /* namespace BLI */