#pragma once

#include "BLI_small_vector.hpp"

namespace BLI {

	template<typename K, typename V, uint N = 4>
	class SmallMap {
	private:
	public:
		/* only public until there are proper iterators */
		struct Entry {
			K key;
			V value;

			Entry() {}
			Entry(K key, V value)
				: key(key), value(value) {}
		};

		SmallVector<Entry> m_entries;

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
	};
};