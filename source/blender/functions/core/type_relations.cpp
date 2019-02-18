#include "type_relations.hpp"

namespace FN {

	ListTypeRelations::ListTypeRelations(
		SharedType &index_type)
		: m_index_type(index_type) {}

	void ListTypeRelations::insert(
		SharedType &base_type,
		SharedType &list_type,
		SharedFunction &get_element)
	{
		BLI_assert(base_type != list_type);

		BLI_assert(get_element->signature().has_interface(
			{list_type, m_index_type},
			{base_type}));

		Relation relation = {
			base_type,
			list_type,
			get_element,
		};

		m_relations.append(relation);
	}

	bool ListTypeRelations::is_list(SharedType &type)
	{
		for (Relation &relation : m_relations) {
			if (relation.list_type == type) {
				return true;
			}
		}
		return false;
	}

	bool ListTypeRelations::is_base(SharedType &type)
	{
		for (Relation &relation : m_relations) {
			if (relation.base_type == type) {
				return true;
			}
		}
		return false;
	}

	SharedType &ListTypeRelations::get_list_of(SharedType &base_type)
	{
		for (Relation &relation : m_relations) {
			if (relation.base_type == base_type) {
				return relation.list_type;
			}
		}
		BLI_assert(false);
		return *(SharedType *)nullptr;
	}

	SharedType &ListTypeRelations::get_base_of(SharedType &list_type)
	{
		for (Relation &relation : m_relations) {
			if (relation.list_type == list_type) {
				return relation.base_type;
			}
		}
		BLI_assert(false);
		return *(SharedType *)nullptr;
	}

} /* namespace FN */