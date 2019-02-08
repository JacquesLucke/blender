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
			m_elements = this->small_buffer();
			m_capacity = N;
			m_size = 0;
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
			this->destruct_elements_and_free_memory();
		}

		SmallVector &operator=(const SmallVector &other)
		{
			if (this == &other) {
				return *this;
			}

			this->destruct_elements_and_free_memory();
			this->copy_from_other(other);

			return *this;
		}

		SmallVector &operator=(SmallVector &&other)
		{
			this->destruct_elements_and_free_memory();
			this->steal_from_other(std::forward<SmallVector>(other));

			return *this;
		}

		void reserve(uint size)
		{
			this->grow(size);
		}

		void append(const T &value)
		{
			this->ensure_space_for_one();
			std::uninitialized_copy(&value, &value + 1, this->end());
			m_size++;
		}

		void append(T &&value)
		{
			this->ensure_space_for_one();
			std::uninitialized_copy(
				std::make_move_iterator(&value),
				std::make_move_iterator(&value + 1),
				this->end());
			m_size++;
		}

		void fill(const T &value)
		{
			for (uint i = 0; i < m_size; i++) {
				m_elements[i] = value;
			}
		}

		uint size() const
		{
			return m_size;
		}

		int index(const T &value) const
		{
			for (uint i = 0; i < m_size; i++) {
				if (m_elements[i] == value) {
					return i;
				}
			}
			return -1;
		}

		T &operator[](const int index) const
		{
			BLI_assert(index >= 0 && index < this->size());
			return m_elements[index];
		}

		T *begin() const
		{ return m_elements; }
		T *end() const
		{ return this->begin() + this->size(); }

		const T *cbegin() const
		{ return this->begin(); }
		const T *cend() const
		{ return this->end(); }

	private:
		T *small_buffer() const
		{
			return (T *)m_small_buffer;
		}

		bool is_small() const
		{
			return m_elements == this->small_buffer();
		}

		inline void ensure_space_for_one()
		{
			if (m_size >= m_capacity) {
				this->grow(std::max(m_capacity * 2, (uint)1));
			}
		}

		void grow(uint min_capacity)
		{
			if (m_capacity >= min_capacity) {
				return;
			}

			m_capacity = min_capacity;

			T *new_array = (T *)std::malloc(sizeof(T) * m_capacity);
			std::uninitialized_copy(
				std::make_move_iterator(this->begin()),
				std::make_move_iterator(this->end()),
				new_array);

			this->destruct_elements_but_keep_memory();

			if (!this->is_small()) {
				std::free(m_elements);
			}

			m_elements = new_array;
		}

		void copy_from_other(const SmallVector &other)
		{
			if (other.is_small()) {
				m_elements = this->small_buffer();
			}
			else {
				m_elements = (T *)std::malloc(sizeof(T) * other.m_capacity);
			}

			std::uninitialized_copy(other.begin(), other.end(), m_elements);
			m_capacity = other.m_capacity;
			m_size = other.m_size;
		}

		void steal_from_other(SmallVector &&other)
		{
			if (other.is_small()) {
				std::uninitialized_copy(
					std::make_move_iterator(other.begin()),
					std::make_move_iterator(other.end()),
					this->small_buffer());
				m_elements = this->small_buffer();
			}
			else {
				m_elements = other.m_elements;
			}

			m_capacity = other.m_capacity;
			m_size = other.m_size;

			other.m_elements = nullptr;
		}

		void destruct_elements_and_free_memory()
		{
			this->destruct_elements_but_keep_memory();
			if (!this->is_small()) {
				/* Can be nullptr when previously stolen. */
				if (m_elements != nullptr) {
					std::free(m_elements);
				}
			}
		}

		void destruct_elements_but_keep_memory()
		{
			for (uint i = 0; i < m_size; i++) {
				(m_elements + i)->~T();
			}
		}

	};

} /* namespace BLI */