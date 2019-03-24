#include "dependencies.hpp"

#include "DEG_depsgraph_build.h"
#include "intern/builder/deg_builder_relations.h"

namespace FN {

	BLI_COMPOSITION_IMPLEMENTATION(DependenciesBody);

	void Dependencies::add_object_transform_dependency(struct Object *object)
	{
		m_transform_dependencies.add(object);
	}

	void Dependencies::update_depsgraph(DepsNodeHandle *deps_node)
	{
		for (struct Object *ob : m_transform_dependencies) {
			DEG_add_object_relation(deps_node, ob, DEG_OB_COMP_TRANSFORM, __func__);
		}
	}

	void Dependencies::add_relations(
		struct DepsgraphRelationBuilderRef *builder_,
		const struct OperationKeyRef *target_)
	{
		auto builder = *(DEG::DepsgraphRelationBuilder *)builder_;
		auto target = *(DEG::OperationKey *)target_;

		for (struct Object *ob : m_transform_dependencies) {
			DEG::OperationKey from_key(
				(ID *)ob,
				DEG::NodeType::TRANSFORM,
				DEG::OperationCode::TRANSFORM_FINAL);

			builder.add_relation(from_key, target, "Function Dependency");
		}
	}

} /* namespace FN */