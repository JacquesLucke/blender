#pragma once

#include "BLI_small_vector.hpp"

namespace BLI {

	template<typename K, typename V, uint N = 4>
	class SmallMap {
	private:
		struct Entry {
			K key;
			V value;

			Entry() {}
			Entry(K key, V value)
				: key(key), value(value) {}
		};

		SmallVector<Entry> m_entries;

	public:
		class ValueIterator;

		SmallMap() = default;

		void add(K key, V value)
		{
			for (Entry &entry : this->m_entries) {
				if (entry.key == key) {
					entry.value = value;
					return;
				}
			}
			this->m_entries.append(Entry(key, value));
		}

		bool contains(K key) const
		{
			for (Entry entry : this->m_entries) {
				if (entry.key == key) {
					return true;
				}
			}
			return false;
		}

		V lookup(const K &key) const
		{
			return this->lookup_ref(key);
		}

		V &lookup_ref(const K &key) const
		{
			V *ptr = this->lookup_ptr(key);
			BLI_assert(ptr);
			return *ptr;
		}

		V *lookup_ptr(const K &key) const
		{
			for (Entry &entry : this->m_entries) {
				if (entry.key == key) {
					return &entry.value;
				}
			}
			return nullptr;
		}

		uint size() const
		{
			return this->m_entries.size();
		}

		ValueIterator values() const
		{
			return ValueIterator(*this);
		}

		class ValueIterator {
		private:
			const SmallMap &m_map;
			uint m_index;

			ValueIterator(const SmallMap &map, uint index)
				: m_map(map), m_index(index) {}
		public:
			ValueIterator(const SmallMap &map)
				: ValueIterator(map, 0) {}

			ValueIterator begin() const
			{
				return ValueIterator(this->m_map, 0);
			}

			ValueIterator end() const
			{
				return ValueIterator(this->m_map, this->m_map.size());
			}

			ValueIterator &operator++()
			{
				this->m_index++;
				return *this;
			}

			ValueIterator operator++(int)
			{
				ValueIterator iterator = *this;
				++*this;
				return iterator;
			}

			bool operator!=(const ValueIterator &iterator) const
			{
				return this->m_index != iterator.m_index;
			}

			V operator*() const
			{
				return this->m_map.m_entries[this->m_index].value;
			}
		};
	};
};