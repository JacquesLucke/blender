#include "type_inferencing.hpp"

namespace FN {
	void Inferencer::finalize_id(uint64_t id, SharedType &type)
	{
		if (m_final_types.contains(id)) {
			BLI_assert(m_final_types.lookup_ref(id) == type);
		}
		else {
			m_final_types.add(id, type);
		}
	}

	void Inferencer::insert_final_type(uint64_t id, SharedType &type)
	{
		this->finalize_id(id, type);
	}

	void Inferencer::insert_type_equality(uint64_t a, uint64_t b)
	{
		m_equalities.append(Link(a, b));
	}

	SharedType &Inferencer::get_final_type(uint64_t id)
	{
		return m_final_types.lookup_ref(id);
	}

	bool Inferencer::has_final_type(uint64_t id)
	{
		return m_final_types.contains(id);
	}

	void Inferencer::inference()
	{
		while (!m_equalities.empty()) {
			for (uint i = 0; i < m_equalities.size(); i++) {
				Link link = m_equalities[i];
				bool done = false;

				if (this->has_final_type(link.a)) {
					this->finalize_id(link.b, this->get_final_type(link.a));
					done = true;
				}
				if (this->has_final_type(link.b)) {
					this->finalize_id(link.a, this->get_final_type(link.b));
					done = true;
				}

				if (done) {
					m_equalities.remove_and_reorder(i);
					i--;
				}
			}
		}
	}

} /* namespace FN */