#pragma once

#include "BLI_small_vector.hpp"
#include "BLI_array_lookup.hpp"

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

		static const K &get_key_from_entry(const Entry &entry)
		{
			return entry.key;
		}

		SmallVector<Entry> m_entries;
		ArrayLookup<K, Entry, get_key_from_entry> m_lookup;

	public:
		class ValueIterator;

		SmallMap() = default;

		bool add(const K &key, const V &value)
		{
			if (this->contains(key)) {
				return false;
			}
			else {
				this->add_new(key, value);
				return true;
			}
		}

		void add_new(const K &key, const V &value)
		{
			BLI_assert(!this->contains(key));
			uint index = m_entries.size();
			Entry entry(key, value);
			m_entries.append(entry);
			m_lookup.add_new(m_entries.begin(), index);
		}

		bool contains(const K &key) const
		{
			return m_lookup.contains(m_entries.begin(), key);
		}

		V pop(const K &key)
		{
			BLI_assert(this->contains(key));
			uint index = m_lookup.find(m_entries.begin(), key);
			V value = m_entries[index].value;

			uint last_index = m_entries.size() - 1;
			if (index == last_index) {
				m_entries.remove_last();
				m_lookup.remove(key, index);
			}
			else {
				m_entries.remove_and_reorder(index);
				m_lookup.remove(key, index);
				K &moved_key = m_entries[index].key;
				m_lookup.update_index(moved_key, last_index, index);
			}
			return value;
		}

		V lookup(const K &key) const
		{
			return this->lookup_ref(key);
		}

		V lookup_default(const K &key, V default_value) const
		{
			if (this->contains(key)) {
				return this->lookup(key);
			}
			else {
				return default_value;
			}
		}

		V &lookup_ref(const K &key) const
		{
			V *ptr = this->lookup_ptr(key);
			BLI_assert(ptr);
			return *ptr;
		}

		V *lookup_ptr(const K &key) const
		{
			int index = m_lookup.find(m_entries.begin(), key);
			if (index >= 0) {
				return &m_entries[index].value;
			}
			else {
				return nullptr;
			}
		}

		uint size() const
		{
			return m_entries.size();
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
				return ValueIterator(m_map, 0);
			}

			ValueIterator end() const
			{
				return ValueIterator(m_map, m_map.size());
			}

			ValueIterator &operator++()
			{
				m_index++;
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
				return m_index != iterator.m_index;
			}

			V operator*() const
			{
				return m_map.m_entries[m_index].value;
			}
		};
	};
};
