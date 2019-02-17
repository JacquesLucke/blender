#include "core.hpp"

namespace FN {

	class ListTypeInfo {
	private:
		SharedType m_base_type;

	public:
		static const char *identifier_in_composition();
		static void free_self(void *value);

		ListTypeInfo(SharedType &base_type)
			: m_base_type(base_type) {}
	};

} /* namespace FN */