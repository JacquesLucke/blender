#include "core.hpp"

namespace FN {

	class Inferencer {
	private:
		struct Link {
			uint64_t a, b;
			Link(uint64_t a, uint64_t b)
				: a(a), b(b) {}
		};

		SmallMap<uint64_t, SharedType> m_final_types;
		SmallVector<Link> m_equalities;

		void finalize_id(uint64_t id, SharedType &type);

	public:
		Inferencer() = default;

		void insert_final_type(uint64_t id, SharedType &type);
		void insert_type_equality(uint64_t a, uint64_t b);

		void inference();

		SharedType &get_final_type(uint64_t id);
		bool has_final_type(uint64_t id);
	};

} /* namespace FN */