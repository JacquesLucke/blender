#pragma once

#include "BLI_utildefines.h"
#include <vector>

namespace BLI {

	template<typename T, uint N = 4>
	class SmallVector {
	private:
		using elements_t = std::vector<T>;
		elements_t elements;

	public:
		using iterator = typename elements_t::iterator;
		using const_iterator = typename elements_t::const_iterator;

		SmallVector() {}

		SmallVector(std::initializer_list<T> values)
		{
			for (T value : values) {
				this->append(value);
			}
		}

		void append(T value)
		{
			this->elements.push_back(value);
		}

		uint size() const
		{
			return this->elements.size();
		}

		T &operator[](const int index)
		{
			BLI_assert(index >= 0 && index < this->size());
			return this->elements[index];
		}

		T operator[](const int index) const
		{
			BLI_assert(index >= 0 && index < this->size());
			return this->elements[index];
		}

		const_iterator begin() const
		{ return this->elements.begin(); }
		const_iterator end() const
		{ return this->elements.end(); }

		const_iterator cbegin() const
		{ return this->elements.cbegin(); }
		const_iterator cend() const
		{ return this->elements.cend(); }
	};

} /* namespace BLI */