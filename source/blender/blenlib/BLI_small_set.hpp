#pragma once

#include "BLI_small_vector.hpp"

namespace BLI {

	template<typename T, uint N = 4>
	class SmallSet {
	protected:
		SmallVector<T> m_entries;

	public:
		SmallSet() = default;

		SmallSet(const std::initializer_list<T> &values)
		{
			for (T value : values) {
				this->add(value);
			}
		}

		SmallSet(const SmallVector<T> &values)
		{
			for (T value : values) {
				this->add(value);
			}
		}

		void add(T value)
		{
			if (!this->contains(value)) {
				this->m_entries.append(value);
			}
		}

		bool contains(T value) const
		{
			for (T entry : this->m_entries) {
				if (entry == value) {
					return true;
				}
			}
			return false;
		}

		uint size() const
		{
			return this->m_entries.size();
		}

		T any() const
		{
			BLI_assert(this->size() > 0);
			return this->m_entries[0];
		}


		/* Iterators */

		T *begin() const
		{
			return this->m_entries.begin();
		}

		T *end() const
		{
			return this->m_entries.end();
		}

		const T *cbegin() const
		{
			return this->m_entries.cbegin();
		}

		const T *cend() const
		{
			return this->m_entries.cend();
		}
	};

} /* namespace BLI */