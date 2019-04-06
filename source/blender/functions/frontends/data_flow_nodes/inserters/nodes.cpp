#include "../registry.hpp"

#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_data_flow_nodes.hpp"

#include "RNA_access.h"

#include "DNA_node_types.h"

namespace FN { namespace DataFlowNodes {

	using namespace Types;

	static SharedFunction get_vectorized_function(
		SharedFunction &original_fn,
		PointerRNA &node_rna,
		SmallVector<const char *> vectorize_prop_names)
	{
		SmallVector<bool> vectorized_inputs;
		for (const char *prop_name : vectorize_prop_names) {
			char state[5];
			BLI_assert(RNA_string_length(&node_rna, prop_name) == strlen("BASE"));
			RNA_string_get(&node_rna, prop_name, state);
			BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

			bool is_vectorized = STREQ(state, "LIST");
			vectorized_inputs.append(is_vectorized);
		}

		if (vectorized_inputs.contains(true)) {
			return Functions::to_vectorized_function(original_fn, vectorized_inputs);
		}
		else {
			return original_fn;
		}
	}

	static void INSERT_object_transforms(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);
		Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
		auto fn = Functions::GET_FN_object_location(object);
		builder.insert_matching_function(fn, bnode);
	}

	static SharedFunction &get_float_math_function(int operation)
	{
		switch (operation)
		{
			case 1: return Functions::GET_FN_add_floats();
			case 2: return Functions::GET_FN_multiply_floats();
			case 3: return Functions::GET_FN_min_floats();
			case 4: return Functions::GET_FN_max_floats();
			case 5: return Functions::GET_FN_sin_float();
			default:
				BLI_assert(false);
				return *(SharedFunction *)nullptr;
		}
	}

	static void INSERT_float_math(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);
		int operation = RNA_enum_get(&rna, "operation");

		SharedFunction fn = get_vectorized_function(
			get_float_math_function(operation),
			rna, {"use_list__a", "use_list__b"});
		builder.insert_matching_function(fn, bnode);
	}

	static SharedFunction &get_vector_math_function(int operation)
	{
		switch (operation)
		{
			case 1: return Functions::GET_FN_add_vectors();
			default:
				BLI_assert(false);
				return *(SharedFunction *)nullptr;
		}
	}

	static void INSERT_vector_math(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);
		int operation = RNA_enum_get(&rna, "operation");

		SharedFunction fn = get_vectorized_function(
			get_vector_math_function(operation),
			rna, {"use_list__a", "use_list__b"});
		builder.insert_matching_function(fn, bnode);
	}

	static void INSERT_clamp(GraphBuilder &builder, bNode *bnode)
	{
		SharedFunction &max_fn = Functions::GET_FN_max_floats();
		SharedFunction &min_fn = Functions::GET_FN_min_floats();

		Node *max_node = builder.insert_function(max_fn, bnode);
		Node *min_node = builder.insert_function(min_fn, bnode);

		builder.insert_link(max_node->output(0), min_node->input(0));
		builder.map_input(max_node->input(0), bnode, 0);
		builder.map_input(max_node->input(1), bnode, 1);
		builder.map_input(min_node->input(1), bnode, 2);
		builder.map_output( min_node->output(0), bnode, 0);
	}

	static void INSERT_get_list_element(GraphBuilder &builder, bNode *bnode)
	{
		SharedType &base_type = builder.query_type_property(bnode, "active_type");
		SharedFunction &fn = Functions::GET_FN_get_list_element(base_type);
		builder.insert_matching_function(fn, bnode);
	}

	static void INSERT_list_length(GraphBuilder &builder, bNode *bnode)
	{
		SharedType &base_type = builder.query_type_property(bnode, "active_type");
		SharedFunction &fn = Functions::GET_FN_list_length(base_type);
		builder.insert_matching_function(fn, bnode);
	}

	static Socket insert_pack_list_sockets(
		GraphBuilder &builder,
		bNode *bnode,
		SharedType &base_type,
		const char *prop_name,
		uint start_index)
	{
		auto &empty_fn = Functions::GET_FN_empty_list(base_type);
		Node *node = builder.insert_function(empty_fn, bnode);

		PointerRNA rna = builder.get_rna(bnode);

		uint index = start_index;
		RNA_BEGIN(&rna, itemptr, prop_name)
		{
			Node *new_node;
			int state = RNA_enum_get(&itemptr, "state");
			if (state == 0) {
				/* single value case */
				auto &append_fn = Functions::GET_FN_append_to_list(base_type);
				new_node = builder.insert_function(append_fn, bnode);
				builder.insert_link(node->output(0), new_node->input(0));
				builder.map_input(new_node->input(1), bnode, index);
			}
			else if (state == 1) {
				/* list case */
				auto &combine_fn = Functions::GET_FN_combine_lists(base_type);
				new_node = builder.insert_function(combine_fn, bnode);
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

		return node->output(0);
	}

	static void INSERT_pack_list(GraphBuilder &builder, bNode *bnode)
	{
		SharedType &base_type = builder.query_type_property(bnode, "active_type");
		Socket packed_list_socket = insert_pack_list_sockets(
			builder, bnode, base_type, "variadic", 0);
		builder.map_output(packed_list_socket, bnode, 0);
	}

	static void INSERT_call(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);

		PointerRNA btree_ptr = RNA_pointer_get(&rna, "function_tree");
		bNodeTree *btree = (bNodeTree *)btree_ptr.id.data;

		if (btree == nullptr) {
			BLI_assert(BLI_listbase_is_empty(&bnode->inputs));
			BLI_assert(BLI_listbase_is_empty(&bnode->outputs));
			return;
		}

		Optional<SharedFunction> fn = generate_function(btree);
		BLI_assert(fn.has_value());
		builder.insert_matching_function(fn.value(), bnode);
	}

	static void INSERT_switch(GraphBuilder &builder, bNode *bnode)
	{
		SharedType &data_type = builder.query_type_property(bnode, "data_type");
		auto fn = Functions::GET_FN_bool_switch(data_type);
		builder.insert_matching_function(fn, bnode);
	}

	static void INSERT_combine_vector(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);
		SharedFunction fn = get_vectorized_function(
			Functions::GET_FN_combine_vector(),
			rna, {"use_list__x", "use_list__y", "use_list__z"});
		builder.insert_matching_function(fn, bnode);
	}

	static void INSERT_separate_vector(GraphBuilder &builder, bNode *bnode)
	{
		PointerRNA rna = builder.get_rna(bnode);
		SharedFunction fn = get_vectorized_function(
			Functions::GET_FN_separate_vector(),
			rna, {"use_list__vector"});
		builder.insert_matching_function(fn, bnode);
	}

	void register_node_inserters(GraphInserters &inserters)
	{
		inserters.reg_node_function("fn_VectorDistanceNode", Functions::GET_FN_vector_distance);
		inserters.reg_node_function("fn_RandomNumberNode", Functions::GET_FN_random_number);
		inserters.reg_node_function("fn_MapRangeNode", Functions::GET_FN_map_range);

		inserters.reg_node_inserter("fn_SeparateVectorNode", INSERT_separate_vector);
		inserters.reg_node_inserter("fn_CombineVectorNode", INSERT_combine_vector);
		inserters.reg_node_inserter("fn_ObjectTransformsNode", INSERT_object_transforms);
		inserters.reg_node_inserter("fn_FloatMathNode", INSERT_float_math);
		inserters.reg_node_inserter("fn_VectorMathNode", INSERT_vector_math);
		inserters.reg_node_inserter("fn_ClampNode", INSERT_clamp);
		inserters.reg_node_inserter("fn_GetListElementNode", INSERT_get_list_element);
		inserters.reg_node_inserter("fn_PackListNode", INSERT_pack_list);
		inserters.reg_node_inserter("fn_CallNode", INSERT_call);
		inserters.reg_node_inserter("fn_SwitchNode", INSERT_switch);
		inserters.reg_node_inserter("fn_ListLengthNode", INSERT_list_length);
	}

} }