#include "dependencies.hpp"

#include "DEG_depsgraph_build.h"

namespace FN {

	void Dependencies::update_depsgraph(DepsNodeHandle *deps_node)
	{
		for (struct Object *ob : m_transform_dependencies) {
			DEG_add_object_relation(deps_node, ob, DEG_OB_COMP_TRANSFORM, __func__);
		}
	}

} /* namespace FN */