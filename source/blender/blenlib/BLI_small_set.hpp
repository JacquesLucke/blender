#pragma once

#include "BLI_small_vector.hpp"
#include "BLI_array_lookup.hpp"

namespace BLI {

	template<
		typename T,
		uint N = 4,
		typename Hash = std::hash<T>>
	class SmallSet {
	protected:
		SmallVector<T> m_elements;
		ArrayLookup<T> m_lookup;

	public:
		SmallSet() = default;

		SmallSet(const SmallVector<T> &values)
		{
			for (const T &value : values) {
				this->add(value);
			}
		}

		SmallSet (const std::initializer_list<T> &values)
		{
			for (const T &value : values) {
				this->add(value);
			}
		}

		uint size() const
		{
			return m_elements.size();
		}

		bool contains(const T &value) const
		{
			return m_lookup.contains(m_elements.begin(), value);
		}

		void add_new(const T &value)
		{
			BLI_assert(!this->contains(value));
			uint index = m_elements.size();
			m_elements.append(value);
			m_lookup.add_new(m_elements.begin(), index);
		}

		bool add(const T &value)
		{
			uint potential_index = m_elements.size();
			bool newly_inserted = m_lookup.add(m_elements.begin(), value, potential_index);
			if (newly_inserted) {
				m_elements.append(value);
			}
			return newly_inserted;
		}

		T pop()
		{
			BLI_assert(this->size() > 0);
			T value = m_elements.pop_last();
			uint index = m_elements.size();
			m_lookup.remove(value, index);
			return value;
		}

		void remove(const T &value)
		{
			BLI_assert(this->contains(value));
			uint index = m_lookup.find(m_elements.begin(), value);

			uint last_index = m_elements.size() - 1;
			if (index == last_index) {
				m_elements.remove_last();
				m_lookup.remove(value, index);
			}
			else {
				m_elements.remove_and_reorder(index);
				m_lookup.remove(value, index);
				T &moved_value = m_elements[index];
				m_lookup.update_index(moved_value, last_index, index);
			}
		}

		T any() const
		{
			BLI_assert(this->size() > 0);
			return m_elements[0];
		}

		T *begin() const
		{
			return m_elements.begin();
		}

		T *end() const
		{
			return m_elements.end();
		}

	};

} /* namespace BLI */
