#pragma once

#include <atomic>
#include <utility>
#include "BLI_utildefines.h"

namespace BLI {

	class RefCountedBase {
	private:
		std::atomic<int> m_refcount;

	protected:
		virtual ~RefCountedBase() {};

		RefCountedBase()
			: m_refcount(1) {}

	public:
		void incref()
		{
			m_refcount.fetch_add(1);
		}

		void decref()
		{
			int new_value = m_refcount.fetch_sub(1) - 1;
			BLI_assert(new_value >= 0);
			if (new_value == 0) {
				delete this;
			}
		}

		int refcount() const
		{
			return m_refcount;
		}
	};

	template<typename T>
	class Shared {
	private:
		T *m_object;

		Shared() = delete;
		Shared(T *object)
			: m_object(object) {}

		inline void incref()
		{
			m_object->incref();
		}

		inline void decref()
		{
			m_object->decref();
		}

	public:
		template<typename ...Args>
		static Shared<T> New(Args&&... args)
		{
			T *object = new T(std::forward<Args>(args)...);
			return Shared<T>(object);
		}

		static Shared<T> FromPointer(T *object)
		{
			return Shared<T>(object);
		}

		Shared(const Shared &other)
		{
			m_object = other.m_object;
			this->incref();
		}

		Shared(Shared &&other)
		{
			m_object = other.m_object;
			other.m_object = nullptr;
		}

		~Shared()
		{
			/* Can be nullptr when previously moved. */
			if (m_object != nullptr) {
				this->decref();
			}
		}

		Shared &operator=(const Shared &other)
		{
			if (m_object == other.m_object) {
				return *this;
			}

			this->decref();
			m_object = other.m_object;
			this->incref();
			return *this;
		}

		Shared &operator=(Shared &&other)
		{
			this->decref();
			m_object = other.m_object;
			other.m_object = nullptr;
			return *this;
		}

		T *ptr() const
		{
			return m_object;
		}

		T *operator->() const
		{
			return this->ptr();
		}

		friend bool operator==(const Shared &a, const Shared &b)
		{
			return a.m_object == b.m_object;
		}

		friend bool operator!=(const Shared &a, const Shared &b)
		{
			return !(a == b);
		}
	};

} /* namespace BLI */