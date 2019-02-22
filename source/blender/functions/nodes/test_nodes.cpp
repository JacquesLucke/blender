#include "nodes.hpp"
#include "BLI_lazy_init.hpp"
#include "RNA_access.h"
#include "BLI_math.h"

namespace FN { namespace Nodes {

	using namespace Types;

	class CombineVector : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector v;
			v.x = fn_in.get<float>(0);
			v.y = fn_in.get<float>(1);
			v.z = fn_in.get<float>(2);
			fn_out.set<Vector>(0, v);
		}
	};

	class SeparateVector : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector v = fn_in.get<Vector>(0);
			fn_out.set<float>(0, v.x);
			fn_out.set<float>(1, v.y);
			fn_out.set<float>(2, v.z);
		}
	};

	class AddFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	class MultiplyFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a * b);
		}
	};

	class MinFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? a : b);
		}
	};

	class MaxFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? b : a);
		}
	};

	class VectorDistance : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector a = fn_in.get<Vector>(0);
			Vector b = fn_in.get<Vector>(1);
			float distance = len_v3v3((float *)&a, (float *)&b);
			fn_out.set<float>(0, distance);
		}
	};

	static uint32_t random_int(uint32_t x)
	{
		x = (x<<13) ^ x;
		return x * (x * x * 15731 + 789221) + 1376312589;
	}

	static float random_float(uint32_t x)
	{
		x = random_int(x);
		return (float)x / 4294967296.0f;
	}

	class RandomNumber : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			uint32_t seed = fn_in.get<int32_t>(0);
			float min = fn_in.get<float>(1);
			float max = fn_in.get<float>(2);
			float result = random_float(seed) * (max - min) + min;
			fn_out.set<float>(0, result);
		}
	};

	class MapRange : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float value = fn_in.get<float>(0);
			float from_min = fn_in.get<float>(1);
			float from_max = fn_in.get<float>(2);
			float to_min = fn_in.get<float>(3);
			float to_max = fn_in.get<float>(4);

			float from_range = from_max - from_min;
			float to_range = to_max - to_min;

			float result;
			if (from_range == 0) {
				result = to_min;
			}
			else {
				float t = (value - from_min) / from_range;
				CLAMP(t, 0.0f, 1.0f);
				result = t * to_range + to_min;
			}

			fn_out.set<float>(0, result);
		}
	};

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
			OutputParameter("Result", get_float_type()),
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

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_random_number_function)
	{
		auto fn = SharedFunction::New("Random Number", Signature({
			InputParameter("Seed", get_int32_type()),
			InputParameter("Min", get_float_type()),
			InputParameter("Max", get_float_type()),
		}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new RandomNumber());
		return fn;
	}

	LAZY_INIT_REF_STATIC__NO_ARG(SharedFunction, get_map_range_function)
	{
		auto fn = SharedFunction::New("Map Range", Signature({
			InputParameter("Value", get_float_type()),
			InputParameter("From Min", get_float_type()),
			InputParameter("From Max", get_float_type()),
			InputParameter("To Min", get_float_type()),
			InputParameter("To Max", get_float_type()),
		}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new MapRange());
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

	static void insert_clamp_node(
		bNodeTree *UNUSED(btree),
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		SharedFunction &max_fn = get_maximum_function();
		SharedFunction &min_fn = get_minimum_function();

		const Node *max_node = graph->insert(max_fn);
		const Node *min_node = graph->insert(min_fn);

		graph->link(max_node->output(0), min_node->input(0));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 0), max_node->input(0));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 1), max_node->input(1));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 2), min_node->input(1));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->outputs, 0), min_node->output(0));
	}

	void initialize_node_inserters()
	{
		register_node_function_getter__no_arg("fn_CombineVectorNode", get_combine_vector_function);
		register_node_function_getter__no_arg("fn_SeparateVectorNode", get_separate_vector_function);
		register_node_function_getter__no_arg("fn_VectorDistanceNode", get_vector_distance_function);
		register_node_function_getter__no_arg("fn_RandomNumberNode", get_random_number_function);
		register_node_function_getter__no_arg("fn_MapRangeNode", get_map_range_function);
		register_node_inserter("fn_ObjectTransformsNode", insert_object_transforms_node);
		register_node_inserter("fn_FloatMathNode", insert_float_math_node);
		register_node_inserter("fn_ClampNode", insert_clamp_node);
	}

} }