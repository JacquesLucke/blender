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
		while (!m_equality_relations.empty() || !m_list_relations.empty()) {
			if (!this->evaluate_equality_relations()) {
				return false;
			}
			if (!this->evaluate_list_relations()) {
				return false;
			}
		}
		return true;
	}

	bool Inferencer::evaluate_equality_relations()
	{
		for (uint i = 0; i < m_equality_relations.size(); i++) {
			EqualityRelation &relation = m_equality_relations[i];

			for (uint64_t id : relation.ids) {
				if (this->has_final_type(id)) {
					if (!this->finalize_ids(relation.ids, this->get_final_type(id))) {
						return false;
					}
					m_equality_relations.remove_and_reorder(i);
					i--;
					break;
				}
			}
		}
		return true;
	}

	bool Inferencer::evaluate_list_relations()
	{
		for (uint i = 0; i < m_list_relations.size(); i++) {
			ListRelation &relation = m_list_relations[i];

			bool done = false;

			for (uint64_t list_id : relation.list_ids) {
				if (this->has_final_type(list_id)) {
					SharedType &list_type = this->get_final_type(list_id);
					if (!m_list_types.is_list(list_type)) {
						return false;
					}
					SharedType &base_type = m_list_types.get_base_of(list_type);
					this->finalize_list_relation(relation, base_type, list_type);
					done = true;
					break;
				}
			}

			if (!done) {
				for (uint64_t base_id : relation.base_ids) {
					if (this->has_final_type(base_id)) {
						SharedType &base_type = this->get_final_type(base_id);
						if (!m_list_types.is_base(base_type)) {
							return false;
						}
						SharedType &list_type = m_list_types.get_list_of(base_type);
						this->finalize_list_relation(relation, base_type, list_type);
						done = true;
						break;
					}
				}
			}

			if (done) {
				m_list_relations.remove_and_reorder(i);
				i--;
			}
		}
		return true;
	}

	bool Inferencer::finalize_list_relation(
		ListRelation &relation,
		SharedType &base_type,
		SharedType &list_type)
	{
		if (!this->finalize_ids(relation.list_ids, list_type)) {
			return false;
		}
		if (!this->finalize_ids(relation.base_ids, base_type)) {
			return false;
		}
		return true;
	}

} /* namespace FN */