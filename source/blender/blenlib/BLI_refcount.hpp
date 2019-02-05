#include <atomic>
#include <utility>

namespace BLI {

	template<typename T>
	class RefCount {
	private:
		struct RefCountedObject {
			RefCountedObject()
				: m_value(nullptr), m_refcount(1) {}

			T *m_value;
			std::atomic<int> m_refcount;
		};

		RefCountedObject *m_object;
		RefCount() = default;

	public:
		template<typename ...Args>
		static RefCount<T> make(Args&&... args)
		{
			auto refcounted = RefCount<T>();
			refcounted.m_object = new RefCountedObject();
			refcounted.m_object->m_value = new T(std::forward<Args>(args)...);
			return refcounted;
		}

		RefCount(const RefCount &other)
		{
			this->m_object = other.m_object;
			this->incref();
		}

		RefCount(const RefCount &&other)
		{
			this->m_object = other.m_object;
			this->incref();
		}

		~RefCount()
		{
			this->decref();
		}

		RefCount &operator=(const RefCount &other)
		{
			if (this->m_object == other->m_object) {
				return *this;
			}

			this->decref();
			this->m_object = other.m_object;
			this->incref();
			return *this;
		}

		RefCount &operator=(const RefCount &&other)
		{
			this->decref();
			this->m_object = other.m_object;
			this->incref();
			return *this;
		}

		void incref()
		{
			std::atomic_fetch_add(&this->m_object->m_refcount, 1);
		}

		void decref()
		{
			int previous_value = std::atomic_fetch_sub(&this->m_object->m_refcount, 1);
			if (previous_value == 1) {
				delete this->m_object->m_value;
				delete this->m_object;
				this->m_object = nullptr;
			}
		}

		int refcount() const
		{
			return this->m_object->m_refcount;
		}

		T *operator->() const
		{
			return this->m_object->m_value;
		}
	};

} /* namespace BLI */