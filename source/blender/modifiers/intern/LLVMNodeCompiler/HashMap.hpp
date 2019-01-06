#include <cstdint>
#include <vector>

template<typename TKey, typename TValue>
class HashMap {
private:
	struct Entry {
		TKey key;
		TValue value;

		Entry(TKey key, TValue value)
			: key(key), value(value) {}
	};

	std::vector<Entry> entries;

public:
	HashMap() {};

	void add(TKey key, TValue value)
	{
		if (!this->contains(key)) {
			this->entries.push_back(Entry(key, value));
		}
	}

	bool contains(TKey key)
	{
		for (Entry entry : this->entries) {
			if (entry.key == key) return true;
		}
		return false;
	}

	bool key_has_value(TKey key, TValue value)
	{
		if (this->contains(key)) {
			return this->lookup(key) == value;
		}
		return false;
	}

	TValue lookup(TKey key)
	{
		for (Entry entry : this->entries) {
			if (entry.key == key) return entry.value;
		}

		assert(!"key not found");
	}
};