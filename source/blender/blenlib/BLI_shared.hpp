#include <atomic>
#include <utility>

namespace BLI {

	template<typename T>
	class RefCounted {
	private:
		T *m_object;
		std::atomic<int> m_refcount;

		~RefCounted() = default;

	public:
		RefCounted(T *object)
			: m_object(object), m_refcount(1) {}

		inline void incref()
		{
			std::atomic_fetch_add(&this->m_refcount, 1);
		}

		inline void decref()
		{
			int previous_value = std::atomic_fetch_sub(&this->m_refcount, 1);
			if (previous_value == 1) {
				delete this->m_object;
				delete this;
			}
		}

		int refcount() const
		{
			return this->m_refcount;
		}

		T *ptr() const
		{
			return this->m_object;
		}
	};

	template<typename T>
	class Shared {
	private:
		RefCounted<T> *m_object;

		Shared() = delete;
		Shared(RefCounted<T> *object)
			: m_object(object) {}

		inline void incref()
		{
			this->m_object->incref();
		}

		inline void decref()
		{
			this->m_object->decref();
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
			RefCounted<T> *refcounted_value = new RefCounted<T>(ptr);
			return Shared<T>(refcounted_value);
		}

		Shared(const Shared &other)
		{
			this->m_object = other.m_object;
			this->incref();
		}

		Shared(Shared &&other)
		{
			this->m_object = other.m_object;
			other.m_object = nullptr;
		}

		~Shared()
		{
			/* Can be nullptr when previously moved. */
			if (this->m_object != nullptr) {
				this->decref();
			}
		}

		Shared &operator=(const Shared &other)
		{
			if (this->m_object == other->m_object) {
				return *this;
			}

			this->decref();
			this->m_object = other.m_object;
			this->incref();
			return *this;
		}

		Shared &operator=(Shared &&other)
		{
			this->decref();
			this->m_object = other.m_object;
			other.m_object = nullptr;
			return *this;
		}

		T *operator->() const
		{
			return this->m_object->ptr();
		}

		RefCounted<T> *refcounter() const
		{
			return this->m_object;
		}

		friend bool operator==(const Shared &a, const Shared &b)
		{
			return a.refcounter()->ptr() == b.refcounter()->ptr();
		}
	};

} /* namespace BLI */