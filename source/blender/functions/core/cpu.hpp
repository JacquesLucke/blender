#pragma once

#include "core.hpp"

namespace FN {

	class Tuple;
	class TupleCallBody;
	class CPPTypeInfo;

	class TupleCallBody {
	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		virtual ~TupleCallBody() {};

		virtual void call(const Tuple &fn_in, Tuple &fn_out) const = 0;
	};

	class CPPTypeInfo {
	public:
		static const char* identifier_in_composition();
		static void free_self(void *value);

		virtual ~CPPTypeInfo() {};

		virtual uint size_of_type() const = 0;
		virtual void destruct_type(void *ptr) const = 0;
		virtual void copy_to_initialized(void *src, void *dst) const = 0;
		virtual void copy_to_uninitialized(void *src, void *dst) const = 0;
	};

	template<typename T>
	class CPPTypeInfoForType : public CPPTypeInfo {
	public:
		virtual uint size_of_type() const override
		{
			return sizeof(T);
		}

		virtual void destruct_type(void *ptr) const override
		{
			T *ptr_ = (T *)ptr;
			ptr_->~T();
		}

		virtual void copy_to_initialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			std::copy(src_, src_ + 1, dst_);
		}

		virtual void copy_to_uninitialized(void *src, void *dst) const override
		{
			T *dst_ = (T *)dst;
			T *src_ = (T *)src;
			std::uninitialized_copy(src_, src_ + 1, dst_);
		}
	};

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