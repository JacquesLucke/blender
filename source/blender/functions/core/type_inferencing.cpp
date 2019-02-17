#include "type_inferencing.hpp"

namespace FN {
	bool Inferencer::finalize_id(uint64_t id, SharedType &type)
	{
		if (m_final_types.contains(id)) {
			bool same_type = m_final_types.lookup_ref(id) == type;
			return same_type;
		}
		else {
			m_final_types.add(id, type);
			return true;
		}
	}

	bool Inferencer::finalize_ids(
		SmallVector<uint64_t> ids,
		SharedType &type)
	{
		for (uint64_t id : ids) {
			if (!this->finalize_id(id, type)) {
				return false;
			}
		}
		return true;
	}

	void Inferencer::insert_final_type(uint64_t id, SharedType &type)
	{
		this->finalize_id(id, type);
	}

	void Inferencer::insert_equality_relation(SmallVector<uint64_t> ids)
	{
		if (ids.size() >= 2) {
			EqualityRelation relation;
			relation.ids = ids;
			m_equality_relations.append(relation);
		}
	}

	void Inferencer::insert_list_relation(
		SmallVector<uint64_t> list_ids,
		SmallVector<uint64_t> base_ids)
	{
		BLI_assert(list_ids.size() > 0);
		ListRelation relation;
		relation.list_ids = list_ids;
		relation.base_ids = base_ids;
		m_list_relations.append(relation);
	}

	SharedType &Inferencer::get_final_type(uint64_t id)
	{
		return m_final_types.lookup_ref(id);
	}

	bool Inferencer::has_final_type(uint64_t id)
	{
		return m_final_types.contains(id);
	}

	bool Inferencer::inference()
	{
		while (!m_equality_relations.empty()) {
			for (uint i = 0; i < m_equality_relations.size(); i++) {
				EqualityRelation group = m_equality_relations[i];

				for (uint64_t id : group.ids) {
					if (this->has_final_type(id)) {
						if (!this->finalize_ids(group.ids, this->get_final_type(id))) {
							return false;
						}
						m_equality_relations.remove_and_reorder(i);
						i--;
						break;
					}
				}
			}
		}

		return true;
	}

} /* namespace FN */