#include "nodes.hpp"
#include "BLI_lazy_init.hpp"
#include "RNA_access.h"
#include "BLI_math.h"

namespace FN { namespace Nodes {

	using namespace Types;

	class CombineVector : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			Vector v;
			v.x = fn_in.get<float>(0);
			v.y = fn_in.get<float>(1);
			v.z = fn_in.get<float>(2);
			fn_out.set<Vector>(0, v);
		}
	};

	class SeparateVector : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			Vector v = fn_in.get<Vector>(0);
			fn_out.set<float>(0, v.x);
			fn_out.set<float>(1, v.y);
			fn_out.set<float>(2, v.z);
		}
	};

	class AddFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	class MultiplyFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a * b);
		}
	};

	class MinFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? a : b);
		}
	};

	class MaxFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? b : a);
		}
	};

	class VectorDistance : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			Vector a = fn_in.get<Vector>(0);
			Vector b = fn_in.get<Vector>(1);
			float distance = len_v3v3((float *)&a, (float *)&b);
			fn_out.set<float>(0, distance);
		}
	};

	class ObjectTransforms : public FN::TupleCallBody {
	private:
		Object *m_object;

	public:
		ObjectTransforms(Object *object)
			: m_object(object) {}

		void call(const FN::Tuple &UNUSED(fn_in), FN::Tuple &fn_out) const override
		{
			if (m_object) {
				Vector position = *(Vector *)m_object->loc;
				fn_out.set<Vector>(0, position);
			}
			else {
				fn_out.set<Vector>(0, Vector());
			}
		}

		void dependencies(Dependencies &deps) const override
		{
			deps.add_object_transform_dependency(m_object);
		}
	};

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_combine_vector_function)
	{
		auto fn = SharedFunction::New("Combine Vector", Signature({
			InputParameter("X", get_float_type()),
			InputParameter("Y", get_float_type()),
			InputParameter("Z", get_float_type()),
		}, {
			OutputParameter("Vector", get_fvec3_type()),
		}));
		fn->add_body(new CombineVector());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_separate_vector_function)
	{
		auto fn = SharedFunction::New("Separate Vector", Signature({
			InputParameter("Vector", get_fvec3_type()),
		}, {
			OutputParameter("X", get_float_type()),
			OutputParameter("Y", get_float_type()),
			OutputParameter("Z", get_float_type()),
		}));
		fn->add_body(new SeparateVector());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_vector_distance_function)
	{
		auto fn = SharedFunction::New("Vector Distance", Signature({
			InputParameter("A", get_fvec3_type()),
			InputParameter("B", get_fvec3_type()),
		}, {
			OutputParameter("Distance", get_float_type()),
		}));
		fn->add_body(new VectorDistance());
		return fn;
	}

	static SharedFunction get_simple_math_function(std::string name)
	{
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("A", get_float_type()),
			InputParameter("B", get_float_type()),
		}, {
			OutputParameter("Vector", get_float_type()),
		}));
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_add_floats_function)
	{
		auto fn = get_simple_math_function("Add Floats");
		fn->add_body(new AddFloats());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_multiply_floats_function)
	{
		auto fn = get_simple_math_function("Multiply Floats");
		fn->add_body(new MultiplyFloats());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_minimum_function)
	{
		auto fn = get_simple_math_function("Minimum");
		fn->add_body(new MinFloats());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_maximum_function)
	{
		auto fn = get_simple_math_function("Maximum");
		fn->add_body(new MaxFloats());
		return fn;
	}

	static void insert_object_transforms_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		PointerRNA ptr;
		RNA_pointer_create(&btree->id, &RNA_Node, bnode, &ptr);
		Object *object = (Object *)RNA_pointer_get(&ptr, "object").id.data;

		auto fn = SharedFunction::New("Object Transforms", Signature({}, {
			OutputParameter("Location", get_fvec3_type()),
		}));
		fn->add_body(new ObjectTransforms(object));
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	static SharedFunction &get_float_math_function(int operation)
	{
		switch (operation)
		{
			case 1: return get_add_floats_function();
			case 2: return get_multiply_floats_function();
			case 3: return get_minimum_function();
			case 4: return get_maximum_function();
			default:
				BLI_assert(false);
				return *(SharedFunction *)nullptr;
		}
	}

	static void insert_float_math_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		PointerRNA ptr;
		RNA_pointer_create(&btree->id, &RNA_Node, bnode, &ptr);
		int operation = RNA_enum_get(&ptr, "operation");

		SharedFunction &fn = get_float_math_function(operation);
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	void initialize_node_inserters()
	{
		register_node_function_getter__no_arg("fn_CombineVectorNode", get_combine_vector_function);
		register_node_function_getter__no_arg("fn_SeparateVectorNode", get_separate_vector_function);
		register_node_function_getter__no_arg("fn_VectorDistanceNode", get_vector_distance_function);
		register_node_inserter("fn_ObjectTransformsNode", insert_object_transforms_node);
		register_node_inserter("fn_FloatMathNode", insert_float_math_node);
	}

} }