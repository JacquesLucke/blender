#include "graph_generation.hpp"

#include "inserters.hpp"
#include "util_wrappers.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN { namespace DataFlowNodes {

	using namespace Types;

	static SharedType &get_type_of_socket(bNodeSocket *bsocket)
	{
		if (STREQ(bsocket->idname, "fn_FloatSocket")) {
			return get_float_type();
		}
		else if (STREQ(bsocket->idname, "fn_IntegerSocket")) {
			return get_int32_type();
		}
		else if (STREQ(bsocket->idname, "fn_VectorSocket")) {
			return get_fvec3_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}

	static bool is_input_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionInputNode");
	}

	static bool is_output_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionOutputNode");
	}

	static void find_interface_nodes(
		bNodeTree *btree,
		bNode **r_input,
		bNode **r_output)
	{
		bNode *input = nullptr;
		bNode *output = nullptr;

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			if (is_input_node(bnode)) input = bnode;
			if (is_output_node(bnode)) output = bnode;
		}

		*r_input = input;
		*r_output = output;
	}

	static void insert_input_node(
		Builder &builder, bNode *bnode)
	{
		OutputParameters outputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			SharedType &type = get_type_of_socket(bsocket);
			outputs.append(OutputParameter(bsocket->name, type));
		}

		auto fn = SharedFunction::New("Function Input", Signature({}, outputs));
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	static void insert_output_node(
		Builder &builder, bNode *bnode)
	{
		InputParameters inputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			SharedType &type = get_type_of_socket(bsocket);
			inputs.append(InputParameter(bsocket->name, type));
		}

		auto fn = SharedFunction::New("Function Output", Signature(inputs, {}));
		Node *node = builder.insert_function(fn);
		builder.map_sockets(node, bnode);
	}

	Optional<FunctionGraph> generate_function_graph(struct bNodeTree *btree)
	{
		auto graph = SharedDataFlowGraph::New();
		SocketMap socket_map;

		Builder builder(graph, socket_map);
		BuilderContext ctx(btree);
		GraphInserters &inserters = get_standard_inserters();

		bNode *input_node;
		bNode *output_node;
		find_interface_nodes(btree, &input_node, &output_node);

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			if (bnode == input_node || bnode == output_node) {
				continue;
			}

			if (!inserters.insert_node(builder, ctx, bnode)) {
				return {};
			}
		}

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		if (input_node != nullptr) {
			insert_input_node(builder, input_node);
			for (bNodeSocket *bsocket : bSocketList(&input_node->outputs)) {
				input_sockets.append(socket_map.lookup(bsocket));
			}
		}
		if (output_node != nullptr) {
			insert_output_node(builder, output_node);
			for (bNodeSocket *bsocket : bSocketList(&output_node->inputs)) {
				output_sockets.append(socket_map.lookup(bsocket));
			}
		}

		for (bNodeLink *blink : bLinkList(&btree->links)) {
			Socket from = socket_map.lookup(blink->fromsock);
			Socket to = socket_map.lookup(blink->tosock);
			if (from.type() != to.type()) {
				return {};
			}
			builder.insert_link(from, to);
		}

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
				Socket socket = socket_map.lookup(bsocket);
				if (!socket.is_linked()) {
					Optional<Socket> new_origin = inserters.insert_socket(builder, ctx, bsocket);
					if (!new_origin.has_value()) {
						return {};
					}
					builder.insert_link(new_origin.value(), socket);
				}
			}
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);
		return fgraph;
	}

} } /* namespace FN::DataFlowNodes */