#pragma once

#include "cpp_types.hpp"

namespace FN {

	class TupleMeta : public RefCountedBase {
	private:
		SmallTypeVector m_types;
		SmallVector<CPPTypeInfo *> m_type_info;
		SmallVector<uint> m_offsets;
		uint m_total_size;

	public:
		TupleMeta(const SmallTypeVector &types = {})
			: m_types(types)
		{
			m_total_size = 0;
			for (const SharedType &type : types) {
				CPPTypeInfo *info = type->extension<CPPTypeInfo>();
				m_offsets.append(m_total_size);
				m_type_info.append(info);
				m_total_size += info->size_of_type();
			}
			m_offsets.append(m_total_size);
		}

		const SmallTypeVector &types() const
		{
			return m_types;
		}

		const SmallVector<CPPTypeInfo *> &type_infos() const
		{
			return m_type_info;
		}

		const SmallVector<uint> &offsets() const
		{
			return m_offsets;
		}

		uint total_data_size() const
		{
			return m_total_size;
		}

		inline uint total_size() const;

		uint element_amount() const
		{
			return m_types.size();
		}

		uint element_size(uint index) const
		{
			return m_offsets[index + 1] - m_offsets[index];
		}
	};

	using SharedTupleMeta = AutoRefCount<TupleMeta>;

	class Tuple {
	public:
		Tuple(SharedTupleMeta meta)
			: m_meta(std::move(meta))
		{
			m_initialized = (bool *)std::calloc(m_meta->element_amount(), sizeof(bool));
			m_data = std::malloc(m_meta->total_data_size());
			m_owns_mem = true;
		}

		Tuple(
			SharedTupleMeta meta,
			void *data,
			bool *initialized,
			bool take_ownership,
			bool was_initialized = false)
			: m_meta(std::move(meta))
		{
			BLI_assert(data != nullptr);
			BLI_assert(initialized != nullptr);
			m_data = data;
			m_initialized = initialized;
			m_owns_mem = take_ownership;
			if (!was_initialized) {
				this->set_all_uninitialized();
			}
		}

		static Tuple &NewInBuffer(
			SharedTupleMeta &meta,
			void *buffer)
		{
			void *tuple_ptr = buffer;
			void *data_ptr = (char *)buffer + sizeof(Tuple);
			void *init_ptr = (char *)data_ptr + meta->total_data_size();
			Tuple *tuple = new(tuple_ptr) Tuple(meta, data_ptr, (bool *)init_ptr, false);
			return *tuple;
		}

		Tuple(SmallTypeVector types)
			: Tuple(SharedTupleMeta::New(types)) {}

		/* Has to be implemented explicitely in the future. */
		Tuple(const Tuple &tuple) = delete;

		~Tuple()
		{
			this->destruct_all();
			if (m_owns_mem) {
				std::free(m_data);
				std::free(m_initialized);
			}
		}

		template<typename T>
		inline void copy_in(uint index, const T &value)
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(sizeof(T) == m_meta->element_size(index));

			if (std::is_trivial<T>::value) {
				std::memcpy(this->element_ptr(index), &value, sizeof(T));
			}
			else {
				T *dst = (T *)this->element_ptr(index);

				if (m_initialized[index]) {
					std::copy_n(&value, 1, dst);
				}
				else {
					std::uninitialized_copy_n(&value, 1, dst);
				}
			}

			m_initialized[index] = true;
		}

		template<typename T>
		inline void move_in(uint index, T &value)
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(sizeof(T) == m_meta->element_size(index));

			T *dst = (T *)this->element_ptr(index);

			if (m_initialized[index]) {
				std::copy_n(std::make_move_iterator(&value), 1, dst);
			}
			else {
				std::uninitialized_copy_n(std::make_move_iterator(&value), 1, dst);
			}

			m_initialized[index] = true;
		}

		inline void move_in__dynamic(uint index, void *src)
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(src != nullptr);

			void *dst = this->element_ptr(index);
			auto *type_info = m_meta->type_infos()[index];

			if (m_initialized[index]) {
				type_info->copy_to_initialized(src, dst);
			}
			else {
				type_info->copy_to_uninitialized(src, dst);
			}

			m_initialized[index] = true;
		}

		template<typename T>
		inline void set(uint index, const T &value)
		{
			static_assert(std::is_trivial<T>::value,
				"this method can be used with trivial types only");
			this->copy_in<T>(index, value);
		}

		template<typename T>
		inline T copy_out(uint index) const
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(sizeof(T) == m_meta->element_size(index));
			BLI_assert(m_initialized[index]);

			return *(T *)this->element_ptr(index);
		}

		template<typename T>
		inline T relocate_out(uint index) const
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(sizeof(T) == m_meta->element_size(index));
			BLI_assert(m_initialized[index]);

			T &value = this->element_ref<T>(index);
			T tmp = std::move(value);
			value.~T();
			m_initialized[index] = false;

			return tmp;
		}

		inline void relocate_out__dynamic(uint index, void *dst) const
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(m_initialized[index]);
			BLI_assert(dst != nullptr);

			void *src = this->element_ptr(index);
			auto *type_info = m_meta->type_infos()[index];
			type_info->copy_to_uninitialized(src, dst);
			type_info->destruct_type(src);
			m_initialized[index] = false;
		}

		template<typename T>
		inline T get(uint index) const
		{
			static_assert(std::is_trivial<T>::value,
				"this method can be used with trivial types only");
			return this->copy_out<T>(index);
		}

		template<typename T>
		inline T &get_ref(uint index) const
		{
			BLI_assert(index < m_meta->element_amount());
			BLI_assert(m_initialized[index]);
			return this->element_ref<T>(index);
		}

		static inline void copy_element(
			const Tuple &from, uint from_index,
			Tuple &to, uint to_index)
		{
			BLI_assert(from.m_initialized[from_index]);
			BLI_assert(from.m_meta->types()[from_index] == to.m_meta->types()[to_index]);

			void *src = from.element_ptr(from_index);
			void *dst = to.element_ptr(to_index);
			CPPTypeInfo *type_info = from.m_meta->type_infos()[from_index];

			if (to.m_initialized[to_index]) {
				type_info->copy_to_initialized(src, dst);
			}
			else {
				type_info->copy_to_uninitialized(src, dst);
				to.m_initialized[to_index] = true;
			}
		}

		inline void init_default(uint index) const
		{
			CPPTypeInfo *type_info = m_meta->type_infos()[index];
			void *ptr = this->element_ptr(index);

			if (m_initialized[index]) {
				type_info->destruct_type(ptr);
			}

			type_info->construct_default(ptr);
			m_initialized[index] = true;
		}

		inline void init_default_all() const
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				this->init_default(i);
			}
		}

		void *data_ptr() const
		{
			return m_data;
		}

		const uint *offsets_ptr() const
		{
			return m_meta->offsets().begin();
		}

		bool all_initialized() const
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				if (!m_initialized[i]) {
					return false;
				}
			}
			return true;
		}

		bool all_uninitialized() const
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				if (m_initialized[i]) {
					return false;
				}
			}
			return true;
		}

		void set_all_initialized()
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				m_initialized[i] = true;
			}
		}

		void set_all_uninitialized()
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				m_initialized[i] = false;
			}
		}

		void destruct_all()
		{
			for (uint i = 0; i < m_meta->element_amount(); i++) {
				if (m_initialized[i]) {
					m_meta->type_infos()[i]->destruct_type(this->element_ptr(i));
					m_initialized[i] = false;
				}
			}
		}

		void print_initialized(std::string name = "");

	private:
		inline void *element_ptr(uint index) const
		{
			return (void *)((char *)m_data + m_meta->offsets()[index]);
		}

		template<typename T>
		inline T &element_ref(uint index) const
		{
			return *(T *)this->element_ptr(index);
		}

		void *m_data;
		bool *m_initialized;
		bool m_owns_mem;
		SharedTupleMeta m_meta;
	};

	inline uint TupleMeta::total_size() const
	{
		return sizeof(Tuple) + this->total_data_size() + this->element_amount();
	}

} /* namespace FN */

#define FN_TUPLE_STACK_ALLOC(name, meta_expr) \
	SharedTupleMeta &name##_meta = (meta_expr); \
	void *name##_buffer = alloca(name##_meta->total_size()); \
	Tuple &name = Tuple::NewInBuffer(name##_meta, name##_buffer);
