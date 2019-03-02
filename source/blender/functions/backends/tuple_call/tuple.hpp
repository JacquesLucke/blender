#pragma once

#include "cpp_types.hpp"

namespace FN {

	class Tuple {
	public:
		Tuple(const SmallTypeVector &types = {})
			: m_types(types)
		{
			int total_size = 0;
			for (const SharedType &type : types) {
				CPPTypeInfo *info = type->extension<CPPTypeInfo>();

				m_offsets.append(total_size);
				m_initialized.append(false);
				m_type_info.append(info);
				total_size += info->size_of_type();
			}
			m_offsets.append(total_size);
			m_data = std::malloc(total_size);
		}

		/* Has to be implemented explicitely in the future. */
		Tuple(const Tuple &tuple) = delete;

		~Tuple()
		{
			for (uint i = 0; i < m_types.size(); i++) {
				m_type_info[i]->destruct_type(this->element_ptr(i));
			}
			std::free(m_data);
		}

		template<typename T>
		inline void set(uint index, const T &value)
		{
			BLI_assert(index < m_types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (std::is_trivial<T>::value) {
				std::memcpy(this->element_ptr(index), &value, sizeof(T));
			}
			else {
				const T *begin = &value;
				const T *end = begin + 1;
				T *dst = (T *)this->element_ptr(index);

				if (m_initialized[index]) {
					std::copy(begin, end, dst);
				}
				else {
					std::uninitialized_copy(begin, end, dst);
					m_initialized[index] = true;
				}
			}
		}

		template<typename T>
		inline const T &get(uint index) const
		{
			BLI_assert(index < m_types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (!std::is_trivial<T>::value) {
				BLI_assert(m_initialized[index]);
			}

			return *(T *)this->element_ptr(index);
		}

		static inline void copy_element(
			const Tuple &from, uint from_index,
			Tuple &to, uint to_index)
		{
			BLI_assert(from.m_types[from_index] == to.m_types[to_index]);

			from.m_type_info[from_index]->copy_to_initialized(
				from.element_ptr(from_index), to.element_ptr(to_index));
		}

		inline void init_default(uint index) const
		{
			m_type_info[index]->construct_default(this->element_ptr(index));
			m_initialized[index] = true;
		}

		inline void init_default_all() const
		{
			for (uint i = 0; i < m_types.size(); i++) {
				this->init_default(i);
			}
		}

		void *data_ptr() const
		{
			return m_data;
		}

		const uint *offsets_ptr() const
		{
			return m_offsets.begin();
		}

	private:
		inline uint element_size(uint index) const
		{
			return m_offsets[index + 1] - m_offsets[index];
		}

		inline void *element_ptr(uint index) const
		{
			return (void *)((char *)m_data + m_offsets[index]);
		}

		SmallTypeVector m_types;
		SmallVector<CPPTypeInfo *> m_type_info;
		SmallVector<uint> m_offsets;
		SmallVector<bool> m_initialized;
		void *m_data;
	};

} /* namespace FN */