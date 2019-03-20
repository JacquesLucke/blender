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

	static void insert_append_list_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		SharedType &base_type = ctx.type_from_rna(bnode, "active_type");
		SharedFunction &fn = Functions::append_to_list(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static void insert_get_list_element_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		SharedType &base_type = ctx.type_from_rna(bnode, "active_type");
		SharedFunction &fn = Functions::get_list_element(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static void insert_combine_lists_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		SharedType &base_type = ctx.type_from_rna(bnode, "active_type");
		SharedFunction &fn = Functions::combine_lists(base_type);
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static void insert_pack_list_node(
		Builder &builder,
		const BuilderContext ctx,
		bNode *bnode)
	{
		SharedType &base_type = ctx.type_from_rna(bnode, "active_type");

		PointerRNA ptr;
		ctx.get_rna(bnode, &ptr);
		int input_amount = RNA_collection_length(&ptr, "variadic");

		auto &empty_fn = Functions::empty_list(base_type);
		Node *node = builder.insert_function(empty_fn);

		int index = 0;
		RNA_BEGIN(&ptr, itemptr, "variadic")
		{
			Node *new_node;
			int state = RNA_enum_get(&itemptr, "state");
			if (state == 0) {
				/* single value case */
				auto &append_fn = Functions::append_to_list(base_type);
				new_node = builder.insert_function(append_fn);
				builder.insert_link(node->output(0), new_node->input(0));
				builder.map_input(new_node->input(1), bnode, index);
			}
			else if (state == 1) {
				/* list case */
				auto &combine_fn = Functions::combine_lists(base_type);
				new_node = builder.insert_function(combine_fn);
				builder.insert_link(node->output(0), new_node->input(0));
				builder.map_input(new_node->input(1), bnode, index);
			}
			else {
				BLI_assert(false);
				new_node = nullptr;
			}
			node = new_node;
			index++;
		}
		RNA_END;

		builder.map_output(node->output(0), bnode, 0);
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
		inserters.reg_node_inserter("fn_PackListNode", insert_pack_list_node);
	}

} }