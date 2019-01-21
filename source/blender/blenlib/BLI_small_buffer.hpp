#pragma once

#include "BLI_utildefines.h"
#include <vector>
#include <cstring>

namespace BLI {

	template<uint N = 16>
	class SmallBuffer {
	public:
		SmallBuffer() {}

		SmallBuffer(int size)
		{
			this->size = size;
			if (size > N) {
				this->buffer = new char[size];
			}
			else {
				this->buffer = this->internal_buffer;
			}
		}

		void copy_in(uint dst, void *src, uint amount)
		{
			BLI_assert(dst + amount < this->size);
			memcpy(this->buffer + dst, src, amount);
		}

		void copy_out(void *dst, uint src, uint amount) const
		{
			BLI_assert(src + amount < this->size);
			memcpy(dst, this->buffer + src, amount);
		}

	private:
		uint size;
		char *buffer;
		char internal_buffer[N];
	};

} /* namespace BLI */