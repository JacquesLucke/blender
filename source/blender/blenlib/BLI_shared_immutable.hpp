#pragma once

#include <atomic>
#include "BLI_utildefines.h"

namespace BLI {

	class SharedImmutable {
	private:
		std::atomic<int> m_users;

		SharedImmutable(SharedImmutable &other) = delete;

	public:
		SharedImmutable()
			: m_users(1) {}

		virtual ~SharedImmutable() {}


		void new_user()
		{
			std::atomic_fetch_add(&m_users, 1);
		}

		void remove_user()
		{
			int previous = std::atomic_fetch_sub(&m_users, 1);
			if (previous == 1) {
				delete this;
			}
		}

		int users() const
		{
			return m_users;
		}

		bool is_mutable() const
		{
			return m_users == 1;
		}

		bool is_immutable() const
		{
			return m_users > 1;
		}

		void assert_mutable() const
		{
			BLI_assert(this->is_mutable());
		}
	};

} /* namespace BLI */