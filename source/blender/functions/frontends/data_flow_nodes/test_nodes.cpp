#include "nodes.hpp"
#include "RNA_access.h"

#include "FN_functions.hpp"
#include "FN_types.hpp"

namespace FN { namespace DataFlowNodes {

	using namespace Types;

	static void insert_object_transforms_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		PointerRNA ptr;
		RNA_pointer_create(&btree->id, &RNA_Node, bnode, &ptr);
		Object *object = (Object *)RNA_pointer_get(&ptr, "object").id.data;

		auto fn = Functions::object_location(object);
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	static SharedFunction &get_float_math_function(int operation)
	{
		switch (operation)
		{
			case 1: return Functions::add_floats();
			case 2: return Functions::multiply_floats();
			case 3: return Functions::min_floats();
			case 4: return Functions::max_floats();
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
		SharedFunction &max_fn = Functions::max_floats();
		SharedFunction &min_fn = Functions::min_floats();

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
		register_node_function_getter__no_arg("fn_CombineVectorNode", Functions::combine_vector);
		register_node_function_getter__no_arg("fn_SeparateVectorNode", Functions::separate_vector);
		register_node_function_getter__no_arg("fn_VectorDistanceNode", Functions::separate_vector);
		register_node_function_getter__no_arg("fn_RandomNumberNode", Functions::random_number);
		register_node_function_getter__no_arg("fn_MapRangeNode", Functions::map_range);
		register_node_inserter("fn_ObjectTransformsNode", insert_object_transforms_node);
		register_node_inserter("fn_FloatMathNode", insert_float_math_node);
		register_node_inserter("fn_ClampNode", insert_clamp_node);
	}

} }