#pragma once

#include "BLI_shared.hpp"

namespace BLI {

	class SharedImmutable : protected RefCountedBase {
	private:
		SharedImmutable(SharedImmutable &other) = delete;

		template<typename> friend class Shared;

	public:
		SharedImmutable()
			: RefCountedBase() {}

		virtual ~SharedImmutable() {}


		void new_user()
		{
			this->incref();
		}

		void remove_user()
		{
			this->decref();
		}

		int users() const
		{
			return this->refcount();
		}

		bool is_mutable() const
		{
			return this->users() == 1;
		}

		bool is_immutable() const
		{
			return this->users() > 1;
		}

		void assert_mutable() const
		{
			BLI_assert(this->is_mutable());
		}
	};

} /* namespace BLI */