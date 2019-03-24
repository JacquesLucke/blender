#pragma once

#include "FN_core.hpp"

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

	class DependenciesBody : public FunctionBody {
	public:
		BLI_COMPOSITION_DECLARATION(DependenciesBody);

		virtual ~DependenciesBody() {}
		virtual void dependencies(Dependencies &deps) const = 0;
	};

} /* namespace FN */