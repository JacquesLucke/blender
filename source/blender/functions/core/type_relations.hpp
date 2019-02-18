#pragma once

#include "core.hpp"
#include "vector"

namespace FN {

	class ListTypeRelations {
	private:
		struct Relation {
			SharedType base_type;
			SharedType list_type;
			SharedFunction get_element;
			SharedFunction set_element;
		};

		SharedType m_index_type;
		SmallVector<Relation> m_relations;

	public:
		ListTypeRelations(SharedType &index_type);

		void insert(
			SharedType &base_type,
			SharedType &list_type,
			SharedFunction &get_element,
			SharedFunction &set_element);
	};

} /* namespace FN */