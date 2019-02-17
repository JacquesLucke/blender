#include "core.hpp"

namespace FN {

	class Inferencer {
	private:
		struct EqualityRelation {
			SmallVector<uint64_t> ids;
		};
		struct ListRelation {
			SmallVector<uint64_t> list_ids, base_ids;
		};

		SmallMap<uint64_t, SharedType> m_final_types;
		SmallVector<EqualityRelation> m_equality_relations;
		SmallVector<ListRelation> m_list_relations;

		bool finalize_id(uint64_t id, SharedType &type);
		bool finalize_ids(SmallVector<uint64_t> ids, SharedType &type);

	public:
		Inferencer() = default;

		void insert_final_type(uint64_t id, SharedType &type);
		void insert_equality_relation(SmallVector<uint64_t> ids);
		void insert_list_relation(
			SmallVector<uint64_t> list_ids,
			SmallVector<uint64_t> base_ids = {});

		bool inference();

		SharedType &get_final_type(uint64_t id);
		bool has_final_type(uint64_t id);
	};

} /* namespace FN */