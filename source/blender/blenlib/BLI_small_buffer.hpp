#pragma once

#include <cstring>
#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"

namespace BLI {

	template<uint N = 16>
	class SmallBuffer : private SmallVector<char, N> {
	public:
		SmallBuffer() {}

		SmallBuffer(uint size)
			: SmallVector<char, N>(size) { }

		inline void copy_in(uint dst, void *src, uint amount)
		{
			BLI_assert(dst + amount <= this->size());
			memcpy(this->begin() + dst, src, amount);
		}

		inline void copy_out(void *dst, uint src, uint amount) const
		{
			BLI_assert(src + amount <= this->size());
			memcpy(dst, this->begin() + src, amount);
		}

		template<uint amount>
		inline void copy_in(uint dst, void *src)
		{
			BLI_assert(dst + amount <= this->size());
			memcpy(this->begin() + dst, src, amount);
		}

		template<uint amount>
		inline void copy_out(uint dst, void *src) const
		{
			BLI_assert(src + amount <= this->size());
			memcpy(dst, this->begin() + src, amount);
		}
	};

} /* namespace BLI */