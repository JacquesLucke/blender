#pragma once

#include "function.hpp"
#include "vector"

namespace FN {

	class ListTypeRelations {
	private:
		struct Relation {
			SharedType base_type;
			SharedType list_type;
			SharedFunction get_element;
		};

		SharedType m_index_type;
		SmallVector<Relation> m_relations;

	public:
		ListTypeRelations(SharedType &index_type);

		void insert(
			SharedType &base_type,
			SharedType &list_type,
			SharedFunction &get_element);

		bool is_list(SharedType &type);
		bool is_base(SharedType &type);

		SharedType &get_list_of(SharedType &base_type);
		SharedType &get_base_of(SharedType &list_type);
	};

} /* namespace FN */