#include "dependencies.hpp"

#include "DEG_depsgraph_build.h"

namespace FN {

	void Dependencies::update_depsgraph(DepsNodeHandle *deps_node)
	{
		for (struct Object *ob : m_transform_dependencies) {
			DEG_add_object_relation(deps_node, ob, DEG_OB_COMP_TRANSFORM, __func__);
		}
	}

	void Dependencies::add_relations(
		DEG::DepsgraphRelationBuilder &builder,
		const DEG::OperationKey &target)
	{
		for (struct Object *ob : m_transform_dependencies) {
			DEG::OperationKey from_key(
				(ID *)ob,
				DEG::NodeType::TRANSFORM,
				DEG::OperationCode::TRANSFORM_FINAL);

			builder.add_relation(from_key, target, "Function Dependency");
		}
	}

} /* namespace FN */