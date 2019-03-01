#include "object_input.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_object_types.h"

namespace FN { namespace Functions {

	using namespace Types;

	class ObjectTransforms : public TupleCallBody {
	private:
		Object *m_object;

	public:
		ObjectTransforms(Object *object)
			: m_object(object) {}

		void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const override
		{
			if (m_object) {
				Vector position = *(Vector *)m_object->loc;
				fn_out.set<Vector>(0, position);
			}
			else {
				fn_out.set<Vector>(0, Vector());
			}
		}
	};

	class ObjectTransformsDependency : public DependenciesBody {
	private:
		Object *m_object;

	public:
		ObjectTransformsDependency(Object *object)
			: m_object(object) {}

		void dependencies(Dependencies &deps) const override
		{
			deps.add_object_transform_dependency(m_object);
		}
	};

	SharedFunction object_location(Object *object)
	{
		auto fn = SharedFunction::New("Object Transforms", Signature({}, {
			OutputParameter("Location", get_fvec3_type()),
		}));
		fn->add_body(new ObjectTransforms(object));
		fn->add_body(new ObjectTransformsDependency(object));
		return fn;
	}

} } /* namespace FN::Functions */