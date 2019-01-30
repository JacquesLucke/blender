#pragma

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
		SmallMap() {}

		void add(K key, V value)
		{
			if (!this->contains(key)) {
				this->m_entries.append(Entry(key, value));
			}
		}

		bool contains(K key) const
		{
			for (const Entry &entry : this->m_entries) {
				if (entry.key == key) {
					return true;
				}
			}
			return false;
		}

		V lookup(K key) const
		{
			for (const Entry &entry : this->m_entries) {
				if (entry.key == key) {
					return entry.value;
				}
			}
			BLI_assert(false);
			return (V){};
		}

		uint size() const
		{
			return this->m_entries.size();
		}
	};
};