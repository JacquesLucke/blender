#pragma once

#include "BLI_small_set.hpp"

namespace BLI {

	template<typename T>
	class SmallSetVector : public SmallSet<T> {
	public:
		SmallSetVector()
			: SmallSet<T>() {}

		SmallSetVector(const std::initializer_list<T> &values)
			: SmallSet<T>(values) {}

		int index(const T &value) const
		{
			for (uint i = 0; i < this->size(); i++) {
				if (this->m_entries[i] == value) {
					return i;
				}
			}
			return -1;
		}

		T operator[](const int index) const
		{
			BLI_assert(index >= 0 && index < this->size());
			return this->m_entries[index];
		}
	};

};