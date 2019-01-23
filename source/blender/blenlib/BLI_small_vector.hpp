#pragma once

#include "BLI_utildefines.h"
#include <cstdlib>
#include <cstring>

namespace BLI {

	template<typename T, uint N = 4>
	class SmallVector {
	private:
		T m_small_buffer[N];
		T *m_elements;
		uint m_size = 0;
		uint m_capacity = N;

	public:
		SmallVector()
		{
			this->m_elements = this->m_small_buffer;
			this->m_capacity = N;
			this->m_size = 0;
		}

		SmallVector(std::initializer_list<T> values)
			: SmallVector()
		{
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

			this->free_own_buffer();
			this->copy_from_other(other);

			return *this;
		}

		SmallVector &operator=(SmallVector &&other)
		{
			this->free_own_buffer();
			this->steal_from_other(std::forward<SmallVector>(other));

			return *this;
		}

		void append(T value)
		{
			if (this->m_size >= this->m_capacity) {
				this->m_capacity *= 2;
				uint new_byte_size = sizeof(T) * this->m_capacity;
				if (this->is_small()) {
					this->m_elements = (T *)std::malloc(new_byte_size);
				}
				else {
					this->m_elements = (T *)std::realloc(this->m_elements, new_byte_size);
				}
			}

			this->m_elements[this->m_size] = value;
			this->m_size++;
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
		bool is_small() const
		{
			return this->m_elements == this->m_small_buffer;
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
				this->m_elements = this->m_small_buffer;
				std::memcpy(this->m_small_buffer, other.m_small_buffer, sizeof(T) * other.m_size);
			}
			else {
				this->m_elements = (T *)std::malloc(sizeof(T) * other.m_capacity);
				std::memcpy(this->m_elements, other.m_elements, other.m_size);
			}

			this->m_capacity = other.m_capacity;
			this->m_size = other.m_size;
		}

		void steal_from_other(SmallVector &&other)
		{
			if (other.is_small()) {
				this->m_elements = this->m_small_buffer;
				std::memcpy(this->m_small_buffer, other.m_small_buffer, sizeof(T) * other.m_size);
			}
			else {
				this->m_elements = other.m_elements;
			}

			this->m_capacity = other.m_capacity;
			this->m_size = other.m_size;

			other.m_elements = nullptr;
		}
	};

} /* namespace BLI */