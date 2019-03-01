#pragma once

#include "BLI_small_set.hpp"

struct Object;
struct DepsNodeHandle;
struct DepsgraphRelationBuilderRef;
struct OperationKeyRef;

namespace FN {
	using namespace BLI;

	class Dependencies {
	private:
		SmallSet<struct Object *> m_transform_dependencies;

	public:
		void add_object_transform_dependency(struct Object *object);

		void update_depsgraph(DepsNodeHandle *deps_node);

		void add_relations(
			struct DepsgraphRelationBuilderRef *builder,
			const struct OperationKeyRef *target);
	};

} /* namespace FN */