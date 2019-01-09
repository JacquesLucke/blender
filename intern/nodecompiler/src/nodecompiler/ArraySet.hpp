#include <cstdint>
#include <vector>

template<typename T>
class
ArraySet {
private:
	using entries_t = std::vector<T>;
	entries_t entries;

public:
	using const_iterator = typename entries_t::const_iterator;

	ArraySet() {};

	ArraySet(std::vector<T> values)
	{
		for (T value : values) {
			this->add(value);
		}
	}


	ArraySet(std::initializer_list<T> values)
	{
		for (T value : values) {
			this->add(value);
		}
	}

	void add(const T value)
	{
		if (!this->contains(value)) {
			this->entries.push_back(value);
		}
	}

	bool contains(const T value) const
	{
		for (T entry : this->entries) {
			if (entry == value) return true;
		}
		return false;
	}

	T get_any() const
	{
		return this[0];
	}

	T operator[](const int index) const
	{
		assert(index >= 0 && index < this->size());
		return this->entries[index];
	}

	ArraySet<T> operator-(ArraySet<T> &other) const
	{
		ArraySet<T> new_set;
		for (T value : this) {
			if (!other.contains(value)) {
				new_set.add(value);
			}
		}
		return new_set;
	}

	uint size() const
	{
		return this->entries.size();
	}

	const_iterator begin() const
	{ return this->entries.begin(); }
	const_iterator end() const
	{ return this->entries.end(); }

	friend std::ostream &operator<<(std::ostream &stream, const ArraySet<T> &array)
	{
		stream << "{" << std::endl;
		for (T entry : array) {
			stream << "  " << entry << std::endl;
		}
		stream << "}" << std::endl;
		return stream;
	}
};