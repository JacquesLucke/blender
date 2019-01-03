#include <cstdint>
#include <vector>

template<typename T>
class HashSet {
private:
    std::vector<T> entries;

public:
    HashSet() {};
    HashSet(std::vector<T> values)
    {
        for (T value : values) {
            this->add(value);
        }
    }

    HashSet(std::initializer_list<T> values)
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

    bool contains(T value)
    {
        for (T entry : this->entries) {
            if (entry == value) return true;
        }
        return false;
    }

    const std::vector<T> &elements()
    {
        return this->entries;
    }
};