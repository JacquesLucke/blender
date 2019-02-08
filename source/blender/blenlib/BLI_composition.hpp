#pragma once

#include "BLI_small_map.hpp"

namespace BLI {

	class Composition {
	public:
		typedef void (*FreeFunction)(void *value);

	private:
		struct Entry {
			void *value;
			FreeFunction free;

			template<typename T>
			Entry(T *value)
				: value((void *)value), free(get_free_function<T>()) {}
		};

	public:
		template<typename T>
		void add(T *value)
		{
			m_elements.add(this->get_key<T>(), Entry(value));
		}

		template<typename T>
		inline T *get() const
		{
			uint64_t key = this->get_key<T>();
			if (m_elements.contains(key)) {
				return (T *)m_elements.lookup(key).value;
			}
			else {
				return nullptr;
			}
		}

		~Composition()
		{
			for (const Entry &entry : m_elements.values()) {
				entry.free(entry.value);
			}
		}

	private:
		template<typename T>
		static uint64_t get_key()
		{
			return (uint64_t)T::identifier_in_composition();
		}

		template<typename T>
		static FreeFunction get_free_function()
		{
			return T::free_self;
		}

		BLI::SmallMap<uint64_t, Entry> m_elements;
	};

} /* namespace BLI */