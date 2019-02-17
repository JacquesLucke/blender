#include "type_relations.hpp"

namespace FN {

	const char *ListTypeInfo::identifier_in_composition()
	{
		return "List Type Info";
	}

	void ListTypeInfo::free_self(void *value)
	{
		ListTypeInfo *v = (ListTypeInfo *)value;
		delete v;
	}

} /* namespace FN */