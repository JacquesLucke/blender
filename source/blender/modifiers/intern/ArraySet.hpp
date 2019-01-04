#include <cstdint>
#include <vector>

template<typename T>
class
ArraySet {
private:
    std::vector<T> entries;

public:

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

    void add(T value)
    {
        if (!this->contains(value)) {
            this->entries.push_back(value);
        }
    }

    bool contains(T value) const
    {
        for (T entry : this->entries) {
            if (entry == value) return true;
        }
        return false;
    }

    const std::vector<T> &elements() const
    {
        return this->entries;
    }

    T operator[](const int index) const
    {
        assert(index >= 0 && index < this->size());
        return this->entries[index];
    }

    uint size() const
    {
        return this->entries.size();
    }
};