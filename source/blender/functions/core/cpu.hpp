#pragma once

#include "core.hpp"

namespace FN {

	class Tuple;
	class TupleCallBody;
	class TypeSize;

	class TupleCallBody {
	public:
		static constexpr const char *identifier = "Tuple Call Body";

		virtual void call(const Tuple &fn_in, Tuple &fn_out) const = 0;
	};

	class TypeSize {
	public:
		static constexpr const char *identifier = "Type Size";

		TypeSize(uint size)
			: m_size(size) {}

		virtual uint size() const
		{
			return this->m_size;
		}

	private:
		uint m_size;
	};

	inline uint get_type_size(const SharedType &type)
	{
		auto extension = type->extension<TypeSize>();
		BLI_assert(extension);
		return extension->size();
	}

	class Tuple {
	public:
		Tuple() = default;

		Tuple(const SmallTypeVector &types)
			: m_types(types)
		{
			int total_size = 0;
			for (const SharedType &type : types) {
				this->m_offsets.append(total_size);
				this->m_initialized.append(false);
				total_size += get_type_size(type);
			}
			this->m_offsets.append(total_size);
			this->data = std::malloc(total_size);
		}

		~Tuple()
		{
			std::free(this->data);
		}

		template<typename T>
		inline void set(uint index, const T &value)
		{
			BLI_assert(index < this->m_types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (std::is_trivial<T>::value) {
				std::memcpy(this->element_ptr(index), &value, sizeof(T));
			}
			else {
				const T *begin = &value;
				const T *end = begin + 1;
				T *dst = (T *)this->element_ptr(index);

				if (this->m_initialized[index]) {
					std::copy(begin, end, dst);
				}
				else {
					std::uninitialized_copy(begin, end, dst);
					this->m_initialized[index] = true;
				}
			}
		}

		template<typename T>
		inline const T &get(uint index) const
		{
			BLI_assert(index < this->m_types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (!std::is_trivial<T>::value) {
				BLI_assert(this->m_initialized[index]);
			}

			return *(T *)this->element_ptr(index);
		}

		static inline void copy_element(
			const Tuple &from, uint from_index,
			Tuple &to, uint to_index)
		{
			/* only works with trivially copyable data types for now */
			BLI_assert(from.m_types[from_index] == to.m_types[to_index]);
			uint size = from.element_size(from_index);
			memcpy(to.element_ptr(to_index), from.element_ptr(from_index), size);
		}

	private:
		inline uint element_size(uint index) const
		{
			return this->m_offsets[index + 1] - this->m_offsets[index];
		}

		inline void *element_ptr(uint index) const
		{
			return (void *)((char *)this->data + this->m_offsets[index]);
		}

		const SmallTypeVector m_types;
		SmallVector<uint> m_offsets;
		SmallVector<bool> m_initialized;
		void *data;
	};

} /* namespace FN */