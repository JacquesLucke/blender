#include "type_relations.hpp"

namespace FN {

	void ListTypeRelations::insert(
		SharedType &base_type,
		SharedType &list_type,
		SharedFunction &get_element,
		SharedFunction &set_element)
	{
		BLI_assert(base_type == list_type);

		BLI_assert(get_element->signature().has_interface(
			{list_type, m_index_type},
			{base_type}));

		BLI_assert(set_element->signature().has_interface(
			{list_type, m_index_type, base_type},
			{list_type}));

		Relation relation = {
			base_type,
			list_type,
			get_element,
			set_element
		};

		m_relations.append(relation);
	}

} /* namespace FN */