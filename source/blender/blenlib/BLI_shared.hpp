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
	class RefCountedPtr : public RefCountedBase {
	private:
		T *m_ptr;

		~RefCountedPtr()
		{
			delete m_ptr;
		}

	public:
		RefCountedPtr(T *object)
			: RefCountedBase(), m_ptr(object) {}

		T *ptr() const
		{
			return m_ptr;
		}
	};

	template<typename T>
	class Shared {
	private:
		RefCountedPtr<T> *m_refcounter;

		Shared() = delete;
		Shared(RefCountedPtr<T> *object)
			: m_refcounter(object) {}

		inline void incref()
		{
			m_refcounter->incref();
		}

		inline void decref()
		{
			m_refcounter->decref();
		}

	public:
		template<typename ...Args>
		static Shared<T> New(Args&&... args)
		{
			T *actual_value = new T(std::forward<Args>(args)...);
			return Shared<T>::FromPointer(actual_value);
		}

		static Shared<T> FromPointer(T *ptr)
		{
			RefCountedPtr<T> *refcounter = new RefCountedPtr<T>(ptr);
			return Shared<T>(refcounter);
		}

		Shared(const Shared &other)
		{
			m_refcounter = other.m_refcounter;
			this->incref();
		}

		Shared(Shared &&other)
		{
			m_refcounter = other.m_refcounter;
			other.m_refcounter = nullptr;
		}

		~Shared()
		{
			/* Can be nullptr when previously moved. */
			if (m_refcounter != nullptr) {
				this->decref();
			}
		}

		Shared &operator=(const Shared &other)
		{
			if (m_refcounter == other.m_refcounter) {
				return *this;
			}

			this->decref();
			m_refcounter = other.m_refcounter;
			this->incref();
			return *this;
		}

		Shared &operator=(Shared &&other)
		{
			this->decref();
			m_refcounter = other.m_refcounter;
			other.m_refcounter = nullptr;
			return *this;
		}

		T *operator->() const
		{
			return m_refcounter->ptr();
		}

		RefCountedPtr<T> *refcounter() const
		{
			return m_refcounter;
		}

		friend bool operator==(const Shared &a, const Shared &b)
		{
			return a.refcounter()->ptr() == b.refcounter()->ptr();
		}

		friend bool operator!=(const Shared &a, const Shared &b)
		{
			return !(a == b);
		}
	};

} /* namespace BLI */