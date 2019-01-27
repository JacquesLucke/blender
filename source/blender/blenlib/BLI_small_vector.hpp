#pragma once

#include "BLI_utildefines.h"
#include <cstdlib>
#include <cstring>
#include <memory>

namespace BLI {

	template<typename T, uint N = 4>
	class SmallVector {
	private:
		char m_small_buffer[sizeof(T) * N];
		T *m_elements;
		uint m_size = 0;
		uint m_capacity = N;

	public:
		SmallVector()
		{
			this->m_elements = this->small_buffer();
			this->m_capacity = N;
			this->m_size = 0;
		}

		SmallVector(uint size)
			: SmallVector()
		{
			this->reserve(size);
			for (uint i = 0; i < size; i++) {
				this->append(T());
			}
		}

		SmallVector(std::initializer_list<T> values)
			: SmallVector()
		{
			this->reserve(values.size());
			for (T value : values) {
				this->append(value);
			}
		}

		SmallVector(const SmallVector &other)
		{
			this->copy_from_other(other);
		}

		SmallVector(SmallVector &&other)
		{
			this->steal_from_other(std::forward<SmallVector>(other));
		}

		~SmallVector()
		{
			this->free_own_buffer();
		}

		SmallVector &operator=(const SmallVector &other)
		{
			if (this == &other) {
				return *this;
			}

			this->destruct_elements();
			this->free_own_buffer();
			this->copy_from_other(other);

			return *this;
		}

		SmallVector &operator=(SmallVector &&other)
		{
			this->destruct_elements();
			this->free_own_buffer();
			this->steal_from_other(std::forward<SmallVector>(other));

			return *this;
		}

		void reserve(uint size)
		{
			this->grow(size);
		}

		void append(T value)
		{
			if (this->m_size >= this->m_capacity) {
				this->grow(std::max(this->m_capacity * 2, (uint)1));
			}

			std::uninitialized_copy(&value, &value + 1, this->end());
			this->m_size++;
		}

		void fill(T value)
		{
			for (uint i = 0; i < this->m_size; i++) {
				this->m_elements[i] = value;
			}
		}

		uint size() const
		{
			return this->m_size;
		}

		T &operator[](const int index)
		{
			BLI_assert(index >= 0 && index < this->size());
			return this->m_elements[index];
		}

		T operator[](const int index) const
		{
			BLI_assert(index >= 0 && index < this->size());
			return this->m_elements[index];
		}

		T *begin() const
		{ return this->m_elements; }
		T *end() const
		{ return this->begin() + this->size(); }

		const T *cbegin() const
		{ return this->begin(); }
		const T *cend() const
		{ return this->end(); }

	private:
		T *small_buffer() const
		{
			return (T *)this->m_small_buffer;
		}

		bool is_small() const
		{
			return this->m_elements == this->small_buffer();
		}

		void grow(uint min_capacity)
		{
			if (this->m_capacity >= min_capacity) {
				return;
			}

			this->m_capacity = min_capacity;

			T *new_array = (T *)std::malloc(sizeof(T) * this->m_capacity);
			std::uninitialized_copy(
				std::make_move_iterator(this->begin()),
				std::make_move_iterator(this->end()),
				new_array);

			this->destruct_elements();

			if (!this->is_small()) {
				std::free(this->m_elements);
			}

			this->m_elements = new_array;
		}

		void free_own_buffer()
		{
			if (!this->is_small()) {
				/* Can be nullptr when previously stolen. */
				if (this->m_elements != nullptr) {
					std::free(this->m_elements);
				}
			}
		}

		void copy_from_other(const SmallVector &other)
		{
			if (other.is_small()) {
				this->m_elements = this->small_buffer();
			}
			else {
				this->m_elements = (T *)std::malloc(sizeof(T) * other.m_capacity);
			}

			std::uninitialized_copy(other.begin(), other.end(), this->m_elements);
			this->m_capacity = other.m_capacity;
			this->m_size = other.m_size;
		}

		void steal_from_other(SmallVector &&other)
		{
			if (other.is_small()) {
				std::uninitialized_copy(
					std::make_move_iterator(other.begin()),
					std::make_move_iterator(other.end()),
					this->small_buffer());
				this->m_elements = this->small_buffer();
			}
			else {
				this->m_elements = other.m_elements;
			}

			this->m_capacity = other.m_capacity;
			this->m_size = other.m_size;

			other.m_elements = nullptr;
		}

		void destruct_elements()
		{
			for (uint i = 0; i < this->m_size; i++) {
				(this->m_elements + i)->~T();
			}
		}
	};

} /* namespace BLI */