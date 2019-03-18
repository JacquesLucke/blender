#include "registry.hpp"

#include "FN_functions.hpp"
#include "FN_types.hpp"

#include "RNA_access.h"

#include "DNA_node_types.h"

namespace FN { namespace DataFlowNodes {

	using namespace Types;

	static void insert_object_transforms_node(
		Builder &builder,
		const BuilderContext &ctx,
		bNode *bnode)
	{
		PointerRNA ptr;
		RNA_pointer_create(ctx.btree_id(), &RNA_Node, bnode, &ptr);
		Object *object = (Object *)RNA_pointer_get(&ptr, "object").id.data;

		auto fn = Functions::object_location(object);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
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
		Builder &builder,
		const BuilderContext &ctx,
		bNode *bnode)
	{
		PointerRNA ptr;
		ctx.get_rna(bnode, &ptr);
		int operation = RNA_enum_get(&ptr, "operation");

		SharedFunction &fn = get_float_math_function(operation);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static void insert_clamp_node(
		Builder &builder,
		const BuilderContext &UNUSED(ctx),
		bNode *bnode)
	{
		SharedFunction &max_fn = Functions::max_floats();
		SharedFunction &min_fn = Functions::min_floats();

		Node *max_node = builder.insert_function(max_fn);
		Node *min_node = builder.insert_function(min_fn);

		builder.insert_link(max_node->output(0), min_node->input(0));
		builder.map_input(max_node->input(0), bnode, 0);
		builder.map_input(max_node->input(1), bnode, 1);
		builder.map_input(min_node->input(1), bnode, 2);
		builder.map_output(min_node->output(0), bnode, 0);
	}

	static SharedFunction &get_function__append(char *base_type)
	{
		if (STREQ(base_type, "Float")) {
			return Functions::append_float();
		}
		else if (STREQ(base_type, "Vector")) {
			return Functions::append_fvec3();
		}
		else if (STREQ(base_type, "Integer")) {
			return Functions::append_int32();
		}
		else {
			BLI_assert(false);
			return *(SharedFunction *)nullptr;
		}
	}

	static void insert_append_list_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		PointerRNA ptr;
		ctx.get_rna(bnode, &ptr);
		char base_type[64];
		RNA_string_get(&ptr, "active_type", base_type);

		SharedFunction &fn = get_function__append(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static SharedFunction &get_function__get_list_element(char *base_type)
	{
		if (STREQ(base_type, "Float")) {
			return Functions::get_float_list_element();
		}
		else if (STREQ(base_type, "Vector")) {
			return Functions::get_fvec3_list_element();
		}
		else if (STREQ(base_type, "Integer")) {
			return Functions::get_int32_list_element();
		}
		else {
			BLI_assert(false);
			return *(SharedFunction *)nullptr;
		}
	}

	static void insert_get_list_element_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		PointerRNA ptr;
		ctx.get_rna(bnode, &ptr);
		char base_type[64];
		RNA_string_get(&ptr, "active_type", base_type);

		SharedFunction &fn = get_function__get_list_element(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static SharedFunction &get_function__combine_lists(char *base_type)
	{
		if (STREQ(base_type, "Float")) {
			return Functions::combine_float_lists();
		}
		else if (STREQ(base_type, "Vector")) {
			return Functions::combine_fvec3_lists();
		}
		else if (STREQ(base_type, "Integer")) {
			return Functions::combine_int32_lists();
		}
		else {
			BLI_assert(false);
			return *(SharedFunction *)nullptr;
		}
	}

	static void insert_combine_lists_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		PointerRNA ptr;
		ctx.get_rna(bnode, &ptr);
		char base_type[64];
		RNA_string_get(&ptr, "active_type", base_type);

		SharedFunction &fn = get_function__combine_lists(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	void register_node_inserters(GraphInserters &inserters)
	{
		inserters.reg_node_function("fn_CombineVectorNode", Functions::combine_vector);
		inserters.reg_node_function("fn_SeparateVectorNode", Functions::separate_vector);
		inserters.reg_node_function("fn_VectorDistanceNode", Functions::separate_vector);
		inserters.reg_node_function("fn_RandomNumberNode", Functions::random_number);
		inserters.reg_node_function("fn_MapRangeNode", Functions::map_range);

		inserters.reg_node_inserter("fn_ObjectTransformsNode", insert_object_transforms_node);
		inserters.reg_node_inserter("fn_FloatMathNode", insert_float_math_node);
		inserters.reg_node_inserter("fn_ClampNode", insert_clamp_node);
		inserters.reg_node_inserter("fn_AppendToListNode", insert_append_list_node);
		inserters.reg_node_inserter("fn_GetListElementNode", insert_get_list_element_node);
		inserters.reg_node_inserter("fn_CombineListsNode", insert_combine_lists_node);
	}

} }