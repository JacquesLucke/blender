#include "BLI_small_set.hpp"

#include "intern/builder/deg_builder_relations.h"

struct Object;
struct DepsNodeHandle;

namespace FN {
	using namespace BLI;

	class Dependencies {
	private:
		SmallSet<struct Object *> m_transform_dependencies;

	public:
		void add_object_transform_dependency(struct Object *object)
		{
			m_transform_dependencies.add(object);
		}

		void update_depsgraph(DepsNodeHandle *deps_node);

		void add_relations(
			DEG::DepsgraphRelationBuilder &builder,
			const DEG::OperationKey &target);
	};

} /* namespace FN */